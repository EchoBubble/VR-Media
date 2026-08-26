// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// interface for audio output classes. This is used
// so that AndroidVulkanVideoImpl doesn't need to
// know about Unreal, so I can keep Unreal and
// Android code separate.
// ------------------------------------------------
#pragma once

#include <chrono>

class IAudioOut
{
  public:
    virtual ~IAudioOut(){};
    virtual void init(int sampleRate, int channels) = 0;
    virtual void initSilent() = 0;
    virtual void close() = 0;
    virtual void sendBuffer(uint8_t *buf, int bufSize, int64_t presentationTimeNS,
                            bool reset = false) = 0;
    virtual int64_t getPresentationTimeNS() = 0; // current presentation time
    virtual bool setVolume(float level) = 0;
    virtual void setPlaying(bool playing) = 0;
    virtual bool setRate(float rate) = 0;

    virtual bool queueSilence(int64_t timeNS, int64_t presentationTimeNS) = 0;

    virtual int64_t getQueuedTimeNS() = 0;
    virtual bool canAcceptSamples() = 0;

    virtual void onSeek(int64_t newPresentationTimeNS, bool resetAudio) = 0;
    virtual void onHasVideoTime(int64_t newPresentationTimeNS) = 0;

    typedef std::chrono::nanoseconds NsDuration;
    typedef std::chrono::time_point<std::chrono::steady_clock, NsDuration> NsTime;

    virtual NsTime getWaitTimeForPresentationTime(int64_t presentationTimeNS,
                                                  int64_t maxDuration) = 0;
};
