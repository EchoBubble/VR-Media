// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// An implementation of IMediaSamples for the new
// v2 video timing in Unreal media player.
//
// With new timing, whilst in theory IMediaSamples
// is a queue, in use  only the most recent sample
// is ever read.
//
// Given we know that, this class dumps any
// previous samples immediately, which allows
// the underlying hardware image to be released as
// soon as possible, which makes video decoding
// work more efficiently.
// ------------------------------------------------

#pragma once

#include "AndroidVulkanTextureSample.h"
#include "IMediaAudioSample.h"
#include "IMediaSamples.h"

#include "UnrealLogging.h"
// for v2 video timing, the sample queue just
// holds a single image
class FVideoMediaSampleHolder : public IMediaSamples
{

  public:
    FVideoMediaSampleHolder() : AudioSampleQueue(512)
    {
        // nothing to do here
    }

    void AddVideo(TSharedRef<AndroidVulkanTextureSample, ESPMode::ThreadSafe> &InSample)
    {
        if (AfterSeekTimeStamp.IsValid())
        {
            AfterSeekTimeStamp = FMediaTimeStamp();
        }
        if (VideoSample.IsValid())
        {
            // force sample texture clear because
            // we're not going to display it
            VideoSample->Clear();
            UE_LOGFMT(LogDirectVideo, VeryVerbose, "Clearing videoframe {0}",
                      VideoSample.GetSharedReferenceCount());
        }
        VideoSample = InSample;
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "CSet video frame {0}", VideoSample.IsValid());
    }

    virtual bool FetchVideo(
        TRange<FTimespan> TimeRange,
        TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> &OutSample) override
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "BFetch video frame {0}", VideoSample.IsValid());
        if (VideoSample.IsValid())
        {
            OutSample = VideoSample;
            VideoSample.Reset();
            return true;
        }
        return false; // override in child classes, if supported
    }
    virtual bool FetchVideo(
        TRange<FMediaTimeStamp> TimeRange,
        TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> &OutSample) override
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "AFetch video frame {0}", VideoSample.IsValid());
        if (VideoSample.IsValid())
        {
            OutSample = VideoSample;
            VideoSample.Reset();
            return true;
        }
        return false; // override in child classes, if supported
    }

    virtual EFetchBestSampleResult FetchBestVideoSampleForTimeRange(
        const TRange<FMediaTimeStamp> &TimeRange,
        TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> &OutSample, bool bReverse)

    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Best sample {0}", VideoSample.IsValid());
        if (VideoSample.IsValid())
        {
            OutSample = VideoSample;
            VideoSample.Reset();
            return EFetchBestSampleResult::Ok;
        }
        else
        {
            return EFetchBestSampleResult::NoSample;
        }
    }

    virtual void FlushSamples() override
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Flush samples {0}", VideoSample.IsValid());
        // override in child classes, if supported
        VideoSample.Reset();
        AudioSampleQueue.RequestFlush();
        FlushCount += 1;
    }

    virtual uint32 GetFlushCount()
    {
        return FlushCount;
    }

    FMediaTimeStamp AfterSeekTimeStamp;

    void SetAfterSeekSampleTime(FMediaTimeStamp TimeStamp)
    {
        AfterSeekTimeStamp = TimeStamp;
    }

    virtual bool PeekVideoSampleTime(FMediaTimeStamp &TimeStamp) override
    {
        if (VideoSample.IsValid())
        {
            TimeStamp = VideoSample->GetTime();
            UE_LOGFMT(LogDirectVideo, VeryVerbose, "Peek timestamp {0}",
                      TimeStamp.Time.GetTotalSeconds());
            return true;
        }
        else
        {
            if (AfterSeekTimeStamp.IsValid())
            {
                TimeStamp = AfterSeekTimeStamp;
                UE_LOGFMT(LogDirectVideo, VeryVerbose, "Peek timestamp after seek {0}",
                          TimeStamp.Time.GetTotalSeconds());
                return true;
            }

            UE_LOGFMT(LogDirectVideo, VeryVerbose, "Peek timestamp returning false");
            return false;
        }
    }

    virtual int32 NumVideoSamples() const override
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Query num samples {0}", VideoSample.IsValid());
        if (VideoSample.IsValid())
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    bool DiscardVideoSamples(const TRange<FMediaTimeStamp> &TimeRange, bool bReverse) override
    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Discard video samples {0}", VideoSample.IsValid());
        return false;
    }

    virtual uint32 PurgeOutdatedVideoSamples(const FMediaTimeStamp &ReferenceTime, bool bReversed,
                                             FTimespan MaxAge) override

    {
        UE_LOGFMT(LogDirectVideo, VeryVerbose, "PurgeOutdatedVideoSamples {0} {1}",
                  VideoSample.IsValid(), ReferenceTime.Time.GetTicks() * 100);
        return 0;
    };

    TSharedPtr<AndroidVulkanTextureSample, ESPMode::ThreadSafe> VideoSample;

    uint32 FlushCount = 0;

    // audio is just handled as normal so it can go via unreal audio system
    void AddAudio(TSharedRef<IMediaAudioSample, ESPMode::ThreadSafe> &InSample)
    {
        if (!AudioSampleQueue.Enqueue(InSample))
        {
            UE_LOGFMT(LogDirectVideo, VeryVerbose, "Audio sample queue full");
        }
    }

    virtual int32 NumAudio() const override

    {
        return AudioSampleQueue.Num();
    }

    virtual bool CanReceiveAudioSamples(uint32 Num) const override
    {
        return AudioSampleQueue.CanAcceptSamples(Num);
    }

    virtual bool FetchAudio(TRange<FMediaTimeStamp> TimeRange,
                            TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> &OutSample) override
    {
        TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> Sample;

        if (!AudioSampleQueue.Peek(Sample))
        {
            return false;
        }

        const FMediaTimeStamp SampleTime = Sample->GetTime();
        const FMediaTimeStamp SampleEndTime = SampleTime + Sample->GetDuration();

        /*        double upper=0.0;
                double lower=0.0;
                if(TimeRange.HasUpperBound()){
                    upper=TimeRange.GetUpperBoundValue().Time.GetTotalMilliseconds();
                }
                if(TimeRange.HasLowerBound()){
                    lower=TimeRange.GetLowerBoundValue().Time.GetTotalMilliseconds();
                }*/

        if (!TimeRange.Overlaps(
                TRange<FMediaTimeStamp>(SampleTime, SampleTime + Sample->GetDuration())))
        {
            //            UE_LOGFMT(LogDirectVideo, VeryVerbose, "Audio sample bad time {0}->{1} not
            //            in {2}->{3}",
            //                SampleTime.Time.GetTotalMilliseconds(),SampleEndTime.Time.GetTotalMilliseconds(),lower,upper);
            return false;
        }

        OutSample = Sample;
        AudioSampleQueue.Pop();

        return true;
    }

    virtual bool FetchAudio(TRange<FTimespan> TimeRange,
                            TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> &OutSample) override
    {
        TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> Sample;

        if (!AudioSampleQueue.Peek(Sample))
        {
            return false;
        }

        const FMediaTimeStamp SampleTime = Sample->GetTime();
        const FMediaTimeStamp SampleEndTime = SampleTime + Sample->GetDuration();

        /*        double upper=0.0;
                double lower=0.0;

                if(TimeRange.HasUpperBound()){
                    upper=TimeRange.GetUpperBoundValue().GetTotalMilliseconds();
                }
                if(TimeRange.HasLowerBound()){
                    lower=TimeRange.GetLowerBoundValue().GetTotalMilliseconds();
                }*/

        if (!TimeRange.Overlaps(
                TRange<FTimespan>(SampleTime.Time, SampleTime.Time + Sample->GetDuration())))
        {
            UE_LOGFMT(LogDirectVideo, VeryVerbose, "Audio sample bad time {0} not in {1}->{2}",
                      SampleTime.Time.GetTotalMilliseconds(),
                      TimeRange.GetLowerBoundValue().GetTotalMilliseconds(),
                      TimeRange.GetUpperBoundValue().GetTotalMilliseconds());
            return false;
        }

        OutSample = Sample;
        AudioSampleQueue.Pop();
        //        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Pop sample {0} {1} in {2}
        //        {3}",SampleTime.Time.GetTotalMilliseconds(),SampleEndTime.Time.GetTotalMilliseconds(),upper,lower);
        return true;
    }

    bool DiscardAudioSamples(const TRange<FMediaTimeStamp> &TimeRange, bool bReverse) override
    {
        double upper = 0.0;
        double lower = 0.0;
        if (TimeRange.HasUpperBound())
        {
            upper = TimeRange.GetUpperBoundValue().Time.GetTotalMilliseconds();
        }
        if (TimeRange.HasLowerBound())
        {
            lower = TimeRange.GetLowerBoundValue().Time.GetTotalMilliseconds();
        }

        UE_LOGFMT(LogDirectVideo, VeryVerbose, "Discarding audio samples {0} {1}", lower, upper);
        return AudioSampleQueue.Discard(TimeRange, bReverse);
    }

    FMediaTimeStampSample GetLastSentAudioTime()
    {
        return AudioSampleQueue.GetAudioTime();
    }

    FMediaAudioSampleQueue AudioSampleQueue;
};
