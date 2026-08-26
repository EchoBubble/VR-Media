// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "IMediaAudioSample.h"
#include "Math/IntPoint.h"
#include "MediaObjectPool.h"
#include "MediaSampleQueue.h"
#include "Misc/Timespan.h"

#include "UnrealLogging.h"

/**
 * Implements a media audio sample for Android raw PCM audio, and allows us to
 * get a callback when the sample is sent to the audio system.
 */
class FAndroidPCMAudioSample : public IMediaAudioSample, public IMediaPoolable
{
  public:
    class ICallback
    {
        friend class FAndroidPCMAudioSample;

      protected:
        virtual ~ICallback() = default;
        virtual void OnAudioSampleSend(FMediaTimeStampSample SampleTime, int64_t duration,
                                       int64_t samples) = 0;
    };
    /** Default constructor. */
    FAndroidPCMAudioSample()
        : Channels(0), Duration(FTimespan::Zero()), SampleRate(0), Time(FTimespan::Zero(), 0),
          Owner(nullptr)
    {
    }

    /** Virtual destructor. */
    virtual ~FAndroidPCMAudioSample()
    {
    }

  public:
    /**
     * Initialize the sample.
     *
     * @param InBuffer The sample's data buffer.
     * @param InSize The size of the sample buffer (in bytes).
     * @param InTime The sample time (relative to presentation clock).
     * @param InDuration The duration for which the sample is valid.
     */
    bool Initialize(ICallback *InOwner, const uint8 *InBuffer, uint32 InSize, uint32 InChannels,
                    uint32 InSampleRate, FMediaTimeStamp InTime, FTimespan InDuration)
    {
        Owner = InOwner;

        Buffer.Reset(InSize);
        if (InSize > 0)
        {
            Buffer.Append(InBuffer, InSize);
        }

        Channels = InChannels;
        Duration = InDuration;
        SampleRate = InSampleRate;
        Time = InTime;
        if ((InBuffer == nullptr) || (InSize == 0))
        {
            return false;
        }

        return true;
    }

  public:
    // IMediaPoolable interface
    virtual void InitializePoolable() override
    {
        Buffer.Reset(0);
        Owner = nullptr;
    }

    virtual bool IsReadyForReuse() override
    {
        return true;
    }

    virtual void ShutdownPoolable() override
    {
        Buffer.Reset(0);
        Owner = nullptr;
    }

    //~ IMediaAudioSample interface
    virtual const void *GetBuffer() override
    {
        if (Buffer.Num() == 0)
        {
            return nullptr;
        }
        if (Owner != nullptr)
        {
            Owner->OnAudioSampleSend(FMediaTimeStampSample(Time, FPlatformTime::Seconds()),
                                     Duration.GetTicks() * 100L, GetFrames());
        }
        return Buffer.GetData();
    }

    virtual uint32 GetChannels() const override
    {
        return Channels;
    }

    virtual FTimespan GetDuration() const override
    {
        return Duration;
    }

    virtual EMediaAudioSampleFormat GetFormat() const override
    {
        return EMediaAudioSampleFormat::Int16;
    }

    virtual uint32 GetFrames() const override
    {
        return Buffer.Num() / (Channels * sizeof(int16));
    }

    virtual uint32 GetSampleRate() const override
    {
        return SampleRate;
    }

    virtual FMediaTimeStamp GetTime() const override
    {
        return Time;
    }

  private:
    /** The sample's data buffer. */
    TArray<uint8> Buffer;

    /** Number of audio channels. */
    uint32 Channels;

    /** The duration for which the sample is valid. */
    FTimespan Duration;

    /** Audio sample rate (in samples per second). */
    uint32 SampleRate;

    /** Presentation time for which the sample was generated. */
    FMediaTimeStamp Time;

    /** Owner callback for sending the sample present time */
    ICallback *Owner;
};

/** Implements a pool for PCM audio sample objects. */
class FAndroidPCMAudioSamplePool : public TMediaObjectPool<FAndroidPCMAudioSample>
{
};
