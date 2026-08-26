// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// A custom file source - used for reading encrypted
// or compressed video files.
// ------------------------------------------------
#pragma once

#include <cstdint>

class ICustomMediaFileSource
{
  public:
    virtual ~ICustomMediaFileSource(){};
    virtual int64_t getAvailableSize(uint64_t offset) = 0;
    virtual int64_t getSize() = 0;
    virtual int64_t readAt(uint64_t offset, void *buffer, uint64_t size) = 0;
    virtual void release() = 0;
};
