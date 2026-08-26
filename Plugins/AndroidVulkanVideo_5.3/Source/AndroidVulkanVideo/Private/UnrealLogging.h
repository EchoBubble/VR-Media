// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Unreal specific log category definition.
// ------------------------------------------------

#pragma once

#include "ILogger.h"

#include "Logging/StructuredLog.h"

// for forced direct logging to android log
#include "android/log.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDirectVideo, Log, All);

class UnrealLogger : public ILogger
{
  public:
    UnrealLogger()
        : very_verbose_bitmask(0xffff), verbose_bitmask(0xffff), bitmask(0xffff),
          error_bitmask(0xffff)
    {
    }

    void LogWithType(int64_t type, const char *fmt, ...) override
    {
        if ((type & bitmask) == 0)
        {
            // don't format the string unless we're going to output it
            return;
        }
        else
        {
            va_list arg_ptr;
            char buffer[1024] = "";
            va_start(arg_ptr, fmt);
            vsnprintf(buffer, 1023, fmt, arg_ptr);
            va_end(arg_ptr);
            if ((type & error_bitmask) != 0)
            {
                // error
                UE_LOG(LogDirectVideo, Error, TEXT("%s"), ANSI_TO_TCHAR(buffer));
            }
            else if ((type & very_verbose_bitmask) != 0)
            {
                UE_LOG(LogDirectVideo, VeryVerbose, TEXT("%s"), ANSI_TO_TCHAR(buffer));
            }
            else
            {
                UE_LOG(LogDirectVideo, Verbose, TEXT("%s"), ANSI_TO_TCHAR(buffer));
            }
        }
    }

    // log an error even in shipping builds
    void ForceErrorLog(const char *str) override
    {
        __android_log_print(ANDROID_LOG_ERROR, "UE", "LogDirectVideo: %s", str);
    }

    void SetLogVisibilityBitmask(int64_t newBitmask)
    {
        very_verbose_bitmask = 0;
        error_bitmask = 1;
        verbose_bitmask = 0;
        bitmask = newBitmask;
        bool veryVerbose = true;
        for (int64_t bit = 1; bit < ILogger::LogTypes::LOGTYPE_MAX; bit *= 2)
        {
            if (veryVerbose)
            {
                very_verbose_bitmask |= bit;
            }
            else
            {
                verbose_bitmask |= bit;
            }
            veryVerbose = !veryVerbose;
        }
        if (!UE_LOG_ACTIVE(LogDirectVideo, Verbose))
        {
            bitmask &= (~verbose_bitmask);
            verbose_bitmask = 0;
        }
        if (!UE_LOG_ACTIVE(LogDirectVideo, VeryVerbose))
        {
            bitmask &= (~very_verbose_bitmask);
            very_verbose_bitmask = 0;
        }
        if (!UE_LOG_ACTIVE(LogDirectVideo, Error))
        {
            bitmask = 0;
        }
    }

  private:
    int64_t very_verbose_bitmask;
    int64_t verbose_bitmask;
    int64_t bitmask;
    int64_t error_bitmask;
};
