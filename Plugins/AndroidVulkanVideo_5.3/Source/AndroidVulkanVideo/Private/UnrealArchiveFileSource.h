// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Read files from Unreal archive
// ------------------------------------------------
#pragma once

#include "ICustomMediaFileSource.h"

#include "CoreMinimal.h"

class UnrealArchiveFileSource : public ICustomMediaFileSource
{
  public:
    explicit UnrealArchiveFileSource(const TSharedRef<FArchive, ESPMode::ThreadSafe> inArchive);
    virtual ~UnrealArchiveFileSource() override{};
    virtual int64_t getAvailableSize(uint64_t offset) override;
    virtual int64_t getSize() override;
    virtual int64_t readAt(uint64_t offset, void *buffer, uint64_t size) override;
    virtual void release() override
    {
        // make sure delete happens in the same compile unit as the constructor is called in
        delete this;
    }

  private:
    TSharedRef<FArchive, ESPMode::ThreadSafe> archive;
    FCriticalSection threadLock;
};
