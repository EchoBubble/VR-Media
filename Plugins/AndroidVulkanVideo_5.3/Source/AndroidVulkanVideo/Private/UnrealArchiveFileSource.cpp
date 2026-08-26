#include "UnrealArchiveFileSource.h"

UnrealArchiveFileSource::UnrealArchiveFileSource(
    const TSharedRef<FArchive, ESPMode::ThreadSafe> inArchive)
    : archive(inArchive)
{
}

int64_t UnrealArchiveFileSource::getAvailableSize(uint64_t offset)
{
    FScopeLock lockCS(&threadLock);
    return getSize() - offset;
}

int64_t UnrealArchiveFileSource::getSize()
{
    FScopeLock lockCS(&threadLock);
    return archive->TotalSize();
}

int64_t UnrealArchiveFileSource::readAt(uint64_t position, void *buffer, uint64_t readSize)
{
    FScopeLock lockCS(&threadLock);
    int64_t fileSize = archive->TotalSize();
    if (fileSize < position)
    {
        return -1; // EOF
    }

    int64_t curPos = archive->Tell();
    if (curPos != position)
    {
        archive->Seek(position);
    }

    if (position + readSize > fileSize)
    {
        readSize = fileSize - position;
    }

    if (readSize > 0)
    {
        archive->Serialize(buffer, readSize);
    }
    return readSize;
}
