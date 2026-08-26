// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Unreal IAudioOut implementation. This uses a
// USoundWaveProcedural to do the output, and also
// a UAudioComponent object. If given a parent
// actor object which has an existing
// UAudioComponent, it uses that. Otherwise it
// makes a new UAudioComponent and holds a strong
// object pointer to it until this class is
// destroyed.
// ------------------------------------------------
#pragma once

#include "Sound/SoundWaveProcedural.h"
#include <chrono>
#include <cstdint>

#include "IAudioOut.h"

#include "AndroidPCMAudioSample.h"

class USoundWaveProceduralWithTiming;
class UAudioComponent;
class UActorComponent;
class FAndroidVulkanMediaPlayer;
class UMediaSoundComponent;

class UnrealAudioOut : public IAudioOut, protected FAndroidPCMAudioSample::ICallback
{
  public:
    explicit UnrealAudioOut(FAndroidVulkanMediaPlayer *owner);
    virtual ~UnrealAudioOut() override;
    virtual void init(int sampleRate, int channels) override;
    virtual void initSilent() override;
    virtual void close() override;
    virtual void sendBuffer(uint8_t *buf, int bufSize, int64_t presentationTimeNS,
                            bool reset = false) override;
    virtual int64_t getPresentationTimeNS() override; // current presentation time
    virtual IAudioOut::NsTime getWaitTimeForPresentationTime(int64_t presentationTimeNS,
                                                             int64_t maxDuration) override;
    virtual bool setVolume(float volume) override;
    virtual void setPlaying(bool playing) override;
    virtual bool setRate(float rate) override;
    virtual void onSeek(int64_t newPresentationTimeNS, bool resetAudio) override;
    virtual int64_t getQueuedTimeNS() override;
    virtual void onHasVideoTime(int64_t newPresentationTimeNS) override;
    virtual bool canAcceptSamples() override;
    virtual bool queueSilence(int64_t timeNS, int64_t presentationTimeNS) override;

    static FMediaTimeStamp MakeTimeStamp(FTimespan PresentationTime, int32_t SeekIndex,
                                         int32_t LoopIndex)
    {
        FMediaTimeStamp SampleTime =
            FMediaTimeStamp(PresentationTime,

                            FMediaTimeStamp::MakeSequenceIndex(SeekIndex, LoopIndex));

        return SampleTime;
    }

    static inline int32_t GetLoopIndex(FMediaTimeStamp SampleTime)
    {

        return static_cast<int32_t>(SampleTime.SequenceIndex & 0xffffffff);
    }

    static inline int32_t GetSeekIndex(FMediaTimeStamp SampleTime)
    {

        return static_cast<int32_t>(SampleTime.SequenceIndex >> 32) - 1;
    }

    static inline int64 GetFullSequenceIndex(FMediaTimeStamp SampleTime)
    {

        return SampleTime.SequenceIndex;
    }

    // time callback from the FAndroidPCMAudioSample::ICallback interface
    // this is called when the sample is sent to the audio output
    // which may have a resampler in front of it, so it isn't perfect
    // but this does at least have a loop index and 'sampled at' time
    // n.b. unreal doesn't give any good way to get the real audio output
    // sample time including loop index, so we just have to handle this carefully
    virtual void OnAudioSampleSend(FMediaTimeStampSample SampleTime, int64_t duration,
                                   int64_t samples) override;

  private:
    void DestroyCreatedMediaSoundComponent();
    void EstimateLatency();
    void QueueSilenceInternal(int64_t numSamples, FMediaTimeStamp SampleTime);

    bool adjustPresentationTimeOffsetForUnrealTime();

    FAndroidVulkanMediaPlayer *owner;
    FAndroidPCMAudioSamplePool AudioSamplePool;
    TWeakObjectPtr<UMediaSoundComponent> createdMediaSoundComponent;
    TWeakObjectPtr<UMediaSoundComponent> existingMediaSoundComponent;

    bool hasTimeOffset;
    int64_t presentationTimeOffset;
    int numChannels;
    int sampleRate;
    bool isPlaying;

    bool afterSeek;

    float playbackRate;

    int64_t pausedPresentationTime;
    int64_t lastQueuedTime;

    FMediaTimeStampSample LastSentSampleTime;
    int64_t LastSentDuration;

    int64_t queueSamples = 0;

    int64_t estLatencyNS = 0;
};
