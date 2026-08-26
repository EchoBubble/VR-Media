#pragma once
#include <cinttypes>

class ILogger
{
  public:
    virtual void LogWithType(int64_t type, const char *fmt, ...) = 0;
    virtual void ForceErrorLog(const char *str) = 0;
    virtual ~ILogger()
    {
    }
    enum LogTypes
    {
        ERROR = 1 << 0,
        TEXTURE_IMPL = 1 << 1,
        TEXTURE_IMPL_VV = 1 << 2,
        DECODER = 1 << 3,
        DECODER_VV = 1 << 4,
        VULKAN_DYNAMIC = 1 << 5,
        VULKAN_DYNAMIC_VV = 1 << 6,
        LOGTYPE_MAX = 1 << 7,
        ALL_LOGS_BITMASK = 0xff
    };
};
