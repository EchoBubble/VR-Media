// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "VRChunkGameInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnVideoChunkReady,
	bool,
	bSuccess
);

/**
 * 
 */
UCLASS()
class CHUNKTEST_API UVRChunkGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/**
	 * 请求下载并挂载一个视频 Chunk。
	 * Manifest 尚未更新完成时，会先记录请求，更新成功后自动开始下载。
	 */
	UFUNCTION(BlueprintCallable, Category = "Video Chunk")
	void RequestVideoChunk(int32 ChunkId);

	/** 下载和挂载全部完成后触发。 */
	UPROPERTY(BlueprintAssignable, Category = "Video Chunk")
	FOnVideoChunkReady OnVideoChunkReady;

	UFUNCTION(BlueprintCallable)
	void DownloadVideoChunk1001();
	
	void MountVideoChunk1001();
private:
	bool bManifestReady = false;
	bool bOperationInProgress = false;

	TArray<int32> PendingChunkIds;

	void StartPendingDownload();
	void HandleManifestUpdated(bool bSuccess);
	void HandleDownloadFinished(bool bSuccess);
	void HandleMountFinished(bool bSuccess);
};
