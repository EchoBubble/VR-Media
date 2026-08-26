#include "VRChunkGameInstance.h"

#include "ChunkDownloader.h"

namespace VRChunkTest
{
    static const FString DeploymentName = TEXT("VRVideoLive");
    static const FString ContentBuildId = TEXT("VRVideoBuild01");
}

void UVRChunkGameInstance::Init()
{
    Super::Init();

    const FString DeploymentName = TEXT("VRVideoLive");
    const FString ContentBuildId = TEXT("VRVideoBuild01");

    TSharedRef<FChunkDownloader> Downloader =
        FChunkDownloader::GetOrCreate();

    // 因为 Manifest 名字是 BuildManifest-Android.txt
    Downloader->Initialize(TEXT("Android"), 1);

    Downloader->LoadCachedBuild(DeploymentName);

    Downloader->UpdateBuild(
        DeploymentName,
        ContentBuildId,
        [this](bool bSuccess)
        {
            HandleManifestUpdated(bSuccess);
        }
    );
}

void UVRChunkGameInstance::Shutdown()
{
    FChunkDownloader::Shutdown();

    Super::Shutdown();
}

void UVRChunkGameInstance::RequestVideoChunk(int32 ChunkId)
{
    if (ChunkId <= 0)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Invalid Chunk ID: %d"),
            ChunkId
        );

        OnVideoChunkReady.Broadcast(false);
        return;
    }

    if (bOperationInProgress)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("A Chunk operation is already in progress.")
        );

        return;
    }

    PendingChunkIds.Reset();
    PendingChunkIds.Add(ChunkId);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Requested video Chunk: %d"),
        ChunkId
    );

    if (bManifestReady)
    {
        StartPendingDownload();
    }
    else
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Waiting for Manifest update...")
        );
    }
}

void UVRChunkGameInstance::DownloadVideoChunk1001()
{
    if (!bManifestReady)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Manifest not ready!")
        );

        return;
    }

    TArray<int32> ChunkIDs;
    ChunkIDs.Add(1001);

    TSharedRef<FChunkDownloader> Downloader =
        FChunkDownloader::GetChecked();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Start downloading Chunk 1001")
    );

    Downloader->DownloadChunks(
        ChunkIDs,
        [this](bool bSuccess)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Download Chunk1001: %s"),
                bSuccess ? TEXT("SUCCESS") : TEXT("FAILED")
            );

            if (bSuccess)
            {
                MountVideoChunk1001();
            }
        },
        1
    );
}

void UVRChunkGameInstance::MountVideoChunk1001()
{
    TSharedRef<FChunkDownloader> Downloader =
        FChunkDownloader::GetChecked();

    FJsonSerializableArrayInt Chunks;
    Chunks.Add(1001);

    Downloader->MountChunks(
        Chunks,
        [this](bool bSuccess)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Mount Chunk1001: %s"),
                bSuccess ? TEXT("SUCCESS") : TEXT("FAILED")
            );

            OnVideoChunkReady.Broadcast(bSuccess);
        }
    );
}

void UVRChunkGameInstance::HandleManifestUpdated(bool bSuccess)
{
    bManifestReady = bSuccess;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Manifest update result: %s"),
        bSuccess ? TEXT("Success") : TEXT("Failed")
    );

    if (!bSuccess)
    {
        bOperationInProgress = false;
        OnVideoChunkReady.Broadcast(false);
        return;
    }

    if (!PendingChunkIds.IsEmpty())
    {
        StartPendingDownload();
    }
}

void UVRChunkGameInstance::StartPendingDownload()
{
    if (!bManifestReady || PendingChunkIds.IsEmpty())
    {
        OnVideoChunkReady.Broadcast(false);
        return;
    }

    bOperationInProgress = true;

    TSharedRef<FChunkDownloader> Downloader =
        FChunkDownloader::GetChecked();

    for (const int32 ChunkId : PendingChunkIds)
    {
        const int32 Status =
            static_cast<int32>(Downloader->GetChunkStatus(ChunkId));

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Chunk %d current status: %d"),
            ChunkId,
            Status
        );
    }

    Downloader->DownloadChunks(
        PendingChunkIds,
        [this](bool bSuccess)
        {
            HandleDownloadFinished(bSuccess);
        },
        1
    );
}

void UVRChunkGameInstance::HandleDownloadFinished(bool bSuccess)
{
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Chunk download result: %s"),
        bSuccess ? TEXT("Success") : TEXT("Failed")
    );

    if (!bSuccess)
    {
        bOperationInProgress = false;
        OnVideoChunkReady.Broadcast(false);
        return;
    }

    TSharedRef<FChunkDownloader> Downloader =
        FChunkDownloader::GetChecked();

    FJsonSerializableArrayInt ChunksToMount;

    for (const int32 ChunkId : PendingChunkIds)
    {
        ChunksToMount.Add(ChunkId);
    }

    Downloader->MountChunks(
        ChunksToMount,
        [this](bool bMountSuccess)
        {
            HandleMountFinished(bMountSuccess);
        }
    );
}

void UVRChunkGameInstance::HandleMountFinished(bool bSuccess)
{
    bOperationInProgress = false;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Chunk mount result: %s"),
        bSuccess ? TEXT("Success") : TEXT("Failed")
    );

    OnVideoChunkReady.Broadcast(bSuccess);
}