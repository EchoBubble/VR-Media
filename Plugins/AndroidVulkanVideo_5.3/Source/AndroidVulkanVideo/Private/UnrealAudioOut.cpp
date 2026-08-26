// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Unreal audio output implementation.
// ------------------------------------------------

#include "UnrealAudioOut.h"

#include "AudioDevice.h"
#include "Components/ActorComponent.h"
#include "Components/AudioComponent.h"
#include "Runtime/Engine/Classes/Sound/AudioSettings.h"
#include "UObject/UObjectIterator.h"
#include <time.h>

#include "IMediaSamples.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaSoundComponent.h"

#include "Engine/engine.h"

#include "UnrealLogging.h"

#include "AndroidVulkanMediaPlayer.h"

#define AUDIO_SAMPLE_CHUNK_SIZE 256

static int64_t _getNSTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_nsec) + (1000000000LL * static_cast<int64_t>(ts.tv_sec));
}

static int64_t _scaleTime(int64_t val, float scalar)
{
    double dt = double(val) * double(scalar);
    return int64_t(dt);
}

UnrealAudioOut::UnrealAudioOut(FAndroidVulkanMediaPlayer *pOwner)
    : hasTimeOffset(false), presentationTimeOffset(0), pausedPresentationTime(-1), numChannels(0),
      sampleRate(0), playbackRate(1.0f), afterSeek(false), isPlaying(false), owner(pOwner),
      createdMediaSoundComponent(NULL)
{
}

void UnrealAudioOut::close()
{
    afterSeek = false;
    numChannels = 0;
    auto AudioQueue = owner->GetSampleQueue();
    if (AudioQueue.IsValid())
    {
        AudioQueue->FlushSamples();
    }
    // close() is also called between sources on the same media player. Keep the
    // sound component attached so the next source can immediately consume audio
    // samples and provide a stable presentation clock.
    existingMediaSoundComponent = NULL;
}

void UnrealAudioOut::DestroyCreatedMediaSoundComponent()
{
    if (createdMediaSoundComponent.IsValid(false, false))
    {
        auto sndComp = TStrongObjectPtr<UMediaSoundComponent>(createdMediaSoundComponent.Get());
        if (sndComp.IsValid())
        {
            sndComp->SetMediaPlayer(nullptr);
            sndComp->UpdatePlayer();
            sndComp->RemoveFromRoot();
            sndComp->DestroyComponent();
        }
        createdMediaSoundComponent.Reset();
    }
    existingMediaSoundComponent.Reset();
}

void UnrealAudioOut::init(int rate, int channels)
{
    close();
    queueSamples = 0;

    // check if we have a mediasoundcomponent already pointing at this player
    // otherwise create one, to keep functionality consistent with the Android default
    // media player (i.e. you don't need a mediasoundcomponent in your level for it to work)
    bool hasMediaSoundComponent = false;
    for (TObjectIterator<UMediaSoundComponent> it; hasMediaSoundComponent == false && it; ++it)
    {
        UMediaPlayer *player = it->GetMediaPlayer();
        if (player != NULL)
        {
            auto iPlayerPtr = player->GetPlayerFacade()->GetPlayer().Get();
            if (iPlayerPtr == owner)
            {
                hasMediaSoundComponent = true;
                existingMediaSoundComponent = *it;
                it->Start();
                UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Found media sound component"));
            }
            else
            {
                UE_LOG(LogDirectVideo, Warning,
                       TEXT("No audio device found for media sound component"));
            }
        }
    }
    if (!hasMediaSoundComponent)
    {
        createdMediaSoundComponent = NewObject<UMediaSoundComponent>();
        for (TObjectIterator<UMediaPlayer> it; it; ++it)
        {
            UMediaPlayer *player = *it;
            auto iPlayerPtr = player->GetPlayerFacade()->GetPlayer().Get();
            if (iPlayerPtr == owner)
            {

                auto sndComp =
                    TStrongObjectPtr<UMediaSoundComponent>(createdMediaSoundComponent.Get());

                if (sndComp.IsValid())
                {
                    sndComp->AddToRoot();
                    sndComp->SetMediaPlayer(player);
                    sndComp->bIsUISound = true;
                    sndComp->Initialize();
                    sndComp->AddClockSink();
                    sndComp->Start();
                    sndComp->UpdatePlayer();
                }
                UE_LOG(LogDirectVideo, Warning,
                       TEXT("Creating media sound component - you might want to manually add one "
                            "to your level."));
                break;
            }
        }
    }

    EstimateLatency();
    playbackRate = 1.0f;

    numChannels = channels;
    sampleRate = rate;
    hasTimeOffset = false;
    presentationTimeOffset = 0;
    lastQueuedTime = -1;
    isPlaying = true;
    LastSentSampleTime.Invalidate();
    LastSentDuration = 0;
    queueSamples = 0;

    UE_LOG(LogDirectVideo, Verbose, TEXT("Audio component: channels %d sampleRate %d"), channels,
           sampleRate);
}

void UnrealAudioOut::EstimateLatency()
{
    auto DevHandle = FAudioDevice::GetMainAudioDevice();
    if (DevHandle.IsValid())
    {
        FAudioDevice *audioDevice = DevHandle.GetAudioDevice();
        auto platformSettings = audioDevice->PlatformSettings;
        auto estLatencySamples =
            platformSettings.CallbackBufferFrameSize * platformSettings.NumBuffers;
        estLatencyNS = (estLatencySamples * 1000000000LL) / platformSettings.SampleRate;
        UE_LOG(LogDirectVideo, VeryVerbose, TEXT("Audio device found:latency %d frames %ld ns"),
               estLatencySamples, estLatencyNS);
    }
    else
    {
        UE_LOG(LogDirectVideo, Warning, TEXT("No audio device found for media sound component"));
        estLatencyNS = 0;
    }
}

void UnrealAudioOut::initSilent()
{
    close();
    UE_LOGFMT(LogDirectVideo, Verbose, "Audio init silent");
    playbackRate = 1.0f;
    hasTimeOffset = false;
    queueSamples = 0;
}

UnrealAudioOut::~UnrealAudioOut()
{
    DestroyCreatedMediaSoundComponent();
    UE_LOGFMT(LogDirectVideo, Verbose, "Audio out destroyed");
}

bool UnrealAudioOut::canAcceptSamples()
{
    auto AudioQueue = owner->GetSampleQueue();
    if (AudioQueue.IsValid())
    {
        return AudioQueue->CanReceiveAudioSamples(1);
    }
    return false;
}

void UnrealAudioOut::QueueSilenceInternal(int64_t numSamples, FMediaTimeStamp SampleTime)
{
    auto AudioQueue = owner->GetSampleQueue();

    if (numSamples <= 0 || numChannels == 0)
    {
        return;
    }
    if (AudioQueue.IsValid() == false)
    {
        return;
    }

    FTimespan Duration = (numSamples * ETimespan::TicksPerSecond) / sampleRate;
    int64_t silenceSize = numSamples * numChannels * sizeof(int16);
    uint8_t *silenceBuffer = new uint8_t[silenceSize];
    memset(silenceBuffer, 0, silenceSize);
    auto sample = AudioSamplePool.AcquireShared();
    UE_LOG(LogDirectVideo, Verbose,
           TEXT("Queued silence sample of size %lld bytes  sequence:%d:%d time:%ld"), silenceSize,
           GetSeekIndex(SampleTime), GetLoopIndex(SampleTime), SampleTime.Time.GetTicks());
    sample->Initialize(this, silenceBuffer, static_cast<int>(silenceSize), numChannels, sampleRate,
                       SampleTime, Duration);
    TSharedRef<IMediaAudioSample> sampleRef = sample;
    AudioQueue->AddAudio(sampleRef);

    queueSamples += numSamples;

    delete[] silenceBuffer;
}

void UnrealAudioOut::sendBuffer(uint8_t *buf, int bufSize, int64_t presentationTimeNS, bool reset)
{
    if (buf == NULL || bufSize == 0)
    {
        return;
    }
    auto AudioQueue = owner->GetSampleQueue();
    int64_t SeekIndex = owner->GetSeekIndex();
    int64_t LoopIndex = owner->GetLoopIndex();

    if (AudioQueue.IsValid())
    {
        FMediaTimeStamp SampleTime = UnrealAudioOut::MakeTimeStamp(
            FTimespan(presentationTimeNS / 100LL), SeekIndex, LoopIndex);
        int totalSamples = (bufSize / sizeof(int16)) / numChannels;
        int offset = 0;
        if (!LastSentSampleTime.IsValid())
        {
            // if we haven't rendered any audio yet then guess that the current queue head is
            // just about to be played
            UE_LOG(LogDirectVideo, Warning,
                   TEXT("Can't adjust presentation time offset, guessing latency based on current "
                        "queue size"));
            int64_t timeInQueue = (queueSamples * 1000000000LL) / sampleRate;
            int64_t estimatedOutputTimeNS = presentationTimeNS - timeInQueue - estLatencyNS;
            auto nowTime = _getNSTime();
            presentationTimeOffset = (estimatedOutputTimeNS - _scaleTime(nowTime, playbackRate));
        }

        while (totalSamples > 0)
        {
            int numSamples = std::min(totalSamples, AUDIO_SAMPLE_CHUNK_SIZE);
            int thisSize = numSamples * numChannels * sizeof(int16);
            uint8_t *thisPtr = buf + offset;
            offset += thisSize;
            totalSamples -= numSamples;

            FTimespan Duration = (numSamples * ETimespan::TicksPerSecond) / (sampleRate);

            auto sample = AudioSamplePool.AcquireShared();
            sample->Initialize(this, thisPtr, thisSize, numChannels, sampleRate, SampleTime,
                               Duration);

            queueSamples += numSamples;
            lastQueuedTime = presentationTimeNS;
            SampleTime += Duration;

            TSharedRef<IMediaAudioSample> sampleRef = sample;
            AudioQueue->AddAudio(sampleRef);
        }
        owner->UpdateSeekStatusOnData();
    }
}

int64_t UnrealAudioOut::getPresentationTimeNS()
{
    if (!hasTimeOffset)
    {
        if (numChannels == 0)
        {
            // silent - initialize time to zero on first query
            // or else we miss a frame on startup
            presentationTimeOffset = -_scaleTime(_getNSTime(), playbackRate);
            hasTimeOffset = true;
        }
        else
        {
            // with audio - needs to send some audio first
            return -1;
        }
    }
    int64_t retVal = _scaleTime(_getNSTime(), playbackRate) + presentationTimeOffset;
    return retVal;
}

IAudioOut::NsTime UnrealAudioOut::getWaitTimeForPresentationTime(int64_t presentationTimeNS,
                                                                 int64_t maxDurationNS)
{
    int64_t thisTime = getPresentationTimeNS();
    int64_t duration = presentationTimeNS - thisTime;
    // give a little slack for processing time
    duration = (duration * 100L) / 98L;
    if (maxDurationNS > 0 && duration > maxDurationNS)
    {
        duration = maxDurationNS;
    }
    NsTime curTime = std::chrono::steady_clock::now();
    if (duration < 0)
    {
        return curTime;
    }

    NsTime waitUntilTime = curTime + NsDuration(_scaleTime(duration, playbackRate));
    return waitUntilTime;
}

bool UnrealAudioOut::setVolume(float volume)
{
    if (numChannels == 0)
    {
        return false;
    }
    if (volume > 1.0)
        volume = 1.0;
    if (volume < 0.0)
        volume = 0.0;
    if (createdMediaSoundComponent.IsValid())
    {
        createdMediaSoundComponent->SetVolumeMultiplier(volume);
    }
    else if (existingMediaSoundComponent != NULL)
    {
        existingMediaSoundComponent->SetVolumeMultiplier(volume);
    }
    return true;
}

void UnrealAudioOut::setPlaying(bool playing)
{
    if (playing == isPlaying)
    {
        return;
    }
    isPlaying = playing;
    if (playing)
    {
        // starting after a pause
        // reset presentation time offset
        if (pausedPresentationTime != -1 && hasTimeOffset)
        {
            presentationTimeOffset =
                pausedPresentationTime - _scaleTime(_getNSTime(), playbackRate);
        }
    }
    else
    {
        // pausing, save current prestime for restore
        pausedPresentationTime = getPresentationTimeNS();
    }
}

bool UnrealAudioOut::adjustPresentationTimeOffsetForUnrealTime()
{
    FTimespan Duration = owner->GetDuration();
    int32_t LoopIndex = owner->GetLoopIndex();
    if (LastSentSampleTime.IsValid())
    {
        double timeSinceRead = FPlatformTime::Seconds() - LastSentSampleTime.SampledAtTime;
        int64_t timeSinceReadNS = static_cast<int64_t>(timeSinceRead * 1000000000.0);

        int64_t lastSentSampleNS = LastSentSampleTime.TimeStamp.Time.GetTicks() * 100LL;
        int64_t currentTimeNS = lastSentSampleNS + timeSinceReadNS;
        // remove the final output latency
        currentTimeNS -= estLatencyNS;

        int32_t SentLoopIndex = GetLoopIndex(LastSentSampleTime.TimeStamp);
        if (SentLoopIndex < LoopIndex)
        {
            UE_LOG(LogDirectVideo, VeryVerbose,
                   TEXT("Adjusting presentation time offset for loop index %d, last sent loop "
                        "index %d"),
                   LoopIndex, SentLoopIndex);
            // we've looped and current sent sample is in previous loop
            // so adjust current time based on duration
            currentTimeNS -= (Duration.GetTicks() * 100LL * (LoopIndex - SentLoopIndex));
        }

        auto nowTime = _getNSTime();
        int64_t previousPresentationTimeOffset = presentationTimeOffset;
        int64_t targetPresentationTimeOffset = currentTimeNS - _scaleTime(nowTime, playbackRate);
        if (!hasTimeOffset ||
            targetPresentationTimeOffset < previousPresentationTimeOffset - 100000000L)
        {
            // looping or starting up - adjust time offset in one big go
            presentationTimeOffset = targetPresentationTimeOffset;
        }
        else
        {
            // not starting, smooth the adjustment
            presentationTimeOffset =
                (targetPresentationTimeOffset + 7 * presentationTimeOffset) / 8;
        }
        int64_t offsetDiff = presentationTimeOffset - previousPresentationTimeOffset;
        hasTimeOffset = true;
        UE_LOG(LogDirectVideo, VeryVerbose,
               TEXT("Adjusted presentation time offset by %f to %ld CurrentTime: %f"),
               (double)(offsetDiff / 1000000000.0), presentationTimeOffset,
               ((double)currentTimeNS) / 1000000000.0);
        return true;
    }
    else
    {
        UE_LOG(LogDirectVideo, Warning,
               TEXT("Can't adjust presentation time offset, no last sent audio time"));
    }
    return false;
}

void UnrealAudioOut::onSeek(int64_t newPresentationTimeNs, bool resetAudio)
{
    if (!resetAudio)
    {
        // looping - add to loop counter
        owner->IncrementLoopIndex();
    }
    if (numChannels == 0)
    {
        auto nowTime = _getNSTime();
        presentationTimeOffset = newPresentationTimeNs - _scaleTime(nowTime, playbackRate);
        UE_LOG(LogDirectVideo, VeryVerbose,
               TEXT("Seeking: pt %ld update offset %ld (rate=%f) now=%ld"), newPresentationTimeNs,
               presentationTimeOffset, playbackRate, nowTime);
        hasTimeOffset = true;
    }
    else
    {
        // clear audio buffers - i.e. seek
        if (resetAudio)
        {
            // after a seek, we may receive either audio or video frames
            // first. Here we clear the audio buffers, and reset the time
            //  offset so that first of either audio or video frame received
            // will reset the clock
            hasTimeOffset = false;
            auto AudioQueue = owner->GetSampleQueue();
            auto SeekIndex = owner->GetSeekIndex();
            if (AudioQueue.IsValid())
            {
                FMediaTimeStamp SampleTime = UnrealAudioOut::MakeTimeStamp(
                    FTimespan(newPresentationTimeNs / 100LL), SeekIndex + 1, 0);

                AudioQueue->FlushSamples();
                AudioQueue->SetAfterSeekSampleTime(SampleTime);
                owner->UpdateSeekStatusOnData();
                LastSentSampleTime.Invalidate();
                LastSentDuration = 0;
                queueSamples = 0;
            }
        }
        else
        {
            // we are looping - don't clear the audio buffer
            // and use unreal time to adjust the offset
            // n.b. the function here takes account of loop index
            // so it should work seamlessly, as we updated loop index
            // up above
            adjustPresentationTimeOffsetForUnrealTime();
        }
    }
}

bool UnrealAudioOut::setRate(float newRate)
{
    if (numChannels != 0)
    {
        if (newRate != 1.0 && newRate != 0.0)
        {
            UE_LOGFMT(LogDirectVideo, Error,
                      "Trying to set playback rate on video with audio, not supported yet");
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        if (newRate > 0)
        {
            if (hasTimeOffset)
            {
                int64_t presTime = getPresentationTimeNS();
                playbackRate = newRate;
                onSeek(presTime, false);
            }
            else
            {
                playbackRate = newRate;
            }
        }
    }
    return false;
}

int64_t UnrealAudioOut::getQueuedTimeNS()
{
    if (numChannels != 0)
    {
        if (lastQueuedTime != -1)
        {
            int64_t nowTime = getPresentationTimeNS();
            if (nowTime < lastQueuedTime)
            {
                return lastQueuedTime - nowTime;
            }
            else
            {
                return -1;
            }
        }
    }
    return -1;
}

void UnrealAudioOut::onHasVideoTime(int64_t newPresentationTimeNS)
{
    if (!hasTimeOffset)
    {
        // if we haven't sent any audio yet, then set current time to this frame time,
        // and let first updating of offset add silence if required
        presentationTimeOffset = newPresentationTimeNS - _scaleTime(_getNSTime(), playbackRate);
        hasTimeOffset = true;
        afterSeek = true;
    }
}

bool UnrealAudioOut::queueSilence(int64_t timeNS, int64_t presentationTimeNS)
{
    int numSamples = (timeNS * sampleRate) / 1000000000LL;
    int64_t SeekIndex = owner->GetSeekIndex();
    int64_t LoopIndex = owner->GetLoopIndex();
    QueueSilenceInternal(
        numSamples, MakeTimeStamp(FTimespan(presentationTimeNS / 100LL), SeekIndex, LoopIndex));
    return true;
}

void UnrealAudioOut::OnAudioSampleSend(FMediaTimeStampSample SampleTime, int64_t durationNS,
                                       int64_t samples)
{
    queueSamples -= samples;
    // check difference between last sent sample and this
    if (LastSentSampleTime.IsValid() && GetFullSequenceIndex(SampleTime.TimeStamp) ==
                                            GetFullSequenceIndex(LastSentSampleTime.TimeStamp))
    {
        int64_t sampleTimeDiff =
            SampleTime.TimeStamp.Time.GetTicks() - LastSentSampleTime.TimeStamp.Time.GetTicks();
        int64_t reportingTimeDiff = static_cast<int64_t>(
            (SampleTime.SampledAtTime - LastSentSampleTime.SampledAtTime) * 1000000000.0);

        if (reportingTimeDiff < sampleTimeDiff / 2)
        {
            // this is probably just filling up a resampler buffer, assume the first one was the
            // more accurate one time wise
            return;
        }
    }
    UE_LOG(LogDirectVideo, VeryVerbose, TEXT("OnAudioSampleSend: SampleTime: %f, duration: %f"),
           (double)(SampleTime.TimeStamp.Time.GetTicks() / 10000000.0),
           (double)(durationNS / 1000000000.0));

    LastSentSampleTime = SampleTime;
    LastSentDuration = durationNS;
    // only adjust time offset when we have things sending
    adjustPresentationTimeOffsetForUnrealTime();
}
