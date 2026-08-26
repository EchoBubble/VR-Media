// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Vulkan texture sample - holds an opaque pointer
// to an Android AHardwareBuffer, and provides the
// IMediaTextureSample and
// IMediaTextureSampleConverter interfaces to allow
// Unreal's media player to show the frame.
// ------------------------------------------------

#pragma once

#include "IMediaPlayer.h"
#include "IMediaTextureSample.h"
#include "IMediaTextureSampleConverter.h"
#include "MediaObjectPool.h"

#include "RHIDefinitions.h"

#include "IVulkanVertexData.h"

#include <array>

#define TMP_REPLACE 5

class IVulkanImpl;

class AndroidVulkanTextureSample : public IMediaTextureSample,
                                   public IMediaTextureSampleConverter,
                                   public IMediaPoolable
{
    friend class FAndroidVulkanTextureSamplePool;

  public:
    AndroidVulkanTextureSample();

    void Init(IVulkanImpl *impl, void *hwImage, int w, int h, FMediaTimeStamp sampleTime);
    void InitNoVideo();

    ~AndroidVulkanTextureSample();

    enum RenderState
    {
        NONE = 0,
        INIT,
        READY,
        SHOWN_MESH,
        COPYING_TEXTURE
    } State;

    RenderState GetRenderState();

    void *GetHWImage() const
    {
        return FrameHWImage;
    }

    bool SetMeshCullMode(ERasterizerCullMode cullMode);

    virtual const void *GetBuffer() override
    {
        return NULL;
    }

    virtual FIntPoint GetDim() const override;
    virtual FTimespan GetDuration() const override
    {
        return FTimespan::Zero();
    }
    virtual EMediaTextureSampleFormat GetFormat() const override
    {
        return VideoTextureFormat;
    }

    virtual FIntPoint GetOutputDim() const override;
    virtual uint32 GetStride() const override;
    virtual FRHITexture *GetTexture() const override;
    virtual IMediaTextureSampleConverter *GetMediaTextureSampleConverter() override;

    virtual FMediaTimeStamp GetTime() const;

    virtual bool IsCacheable() const
    {
        return true;
    }
    virtual bool IsOutputSrgb() const
    {
        return false;
    }

    // IMediaTextureSampleConverter interface (changed in 5.5 to add command list)

    virtual bool Convert(FTexture2DRHIRef &InDstTexture, const FConversionHints &Hints);

    bool ConvertInternal(FRHICommandListImmediate &RHICmdList, FTexture2DRHIRef &InDstTexture,
                         const FConversionHints &Hints);

    // called inside render pass
    bool RenderToMesh(FTexture2DRHIRef InDstTexture, FRHICommandList &RHICmdList, void *MeshID);

    // called outside a render pass
    // n.b. don't make matrices etc. const references
    // because then we get dangling references and
    // BAD THINGS happen
    bool UpdateViewMatrices(FTexture2DRHIRef InDstTexture, FRHICommandList &RHICmdList,
                            ShaderViewMatrices Matrices, ShaderModelMatrix Matrix, void *MeshID);
    bool InitFrameForMeshRendering(FTexture2DRHIRef InDstTexture, FRHICommandList &RHICmdList,
                                   void *MeshID, int ImplStereoMode);
    bool UpdateMesh(FTexture2DRHIRef InDstTexture, FRHICommandList &RHICmdList,
                    TArray<VertexData> PositionAndUV, TArray<uint32> Indices,
                    ShaderModelMatrix Matrix, void *MeshID);

    void InitializePoolable() override;
    virtual bool IsReadyForReuse() override;
    virtual void ShutdownPoolable() override;

    void Clear();
    void ClearIfShown();

    static void SetVideoFormat(EMediaTextureSampleFormat Format)
    {
        VideoTextureFormat = Format;
    }

    static void CaptureRenderPass(IMediaPlayer *Player, FRHICommandList &RHICmdList);

    void ImplDeleted();

  private:
    // These are all static to avoid problems with render thread access to the impl if we
    // are cleared while the render thread is still using it.
    static void FRenderOnRHIThreadCommand(IVulkanImpl *CopyImpl, void *CopyHwImage,
                                          const FTexture2DRHIRef &DstTexture);
    static void FRenderMeshOnRHIThreadCommand(IVulkanImpl *CopyImpl, void *CopyHwImage,
                                              const FTexture2DRHIRef &DstTexture, void *MeshID,
                                              int Samples);
    static void FUpdateMeshOnRHIThreadCommand(IVulkanImpl *CopyImpl,
                                              const FTexture2DRHIRef &DstTexture,
                                              const TArray<VertexData> &PositionAndUV,
                                              const TArray<uint32> &Indices,
                                              const ShaderModelMatrix &Matrix, void *MeshID);
    static void FUpdateMatricesOnRHIThreadCommand(IVulkanImpl *CopyImpl,
                                                  const FTexture2DRHIRef &DstTexture,
                                                  const ShaderViewMatrices &Matrices,
                                                  const ShaderModelMatrix &Matrix, void *MeshID);
    static void FInitMeshRenderingCommand(IVulkanImpl *CopyImpl, void *HWImageCopy,
                                          const FTexture2DRHIRef &DstTexture, void *MeshID,
                                          int Samples);
    bool IsEmptyTexture;
    FGPUFenceRHIRef ShowFence;
    FGPUFenceRHIRef MeshInitFence;
    bool UsedFence0;
    bool UsedFence1;

    // for mesh rendering we need as many fences as we have output views (i.e. swapchain buffers)
    // as fences don't always get reset earlier than the next use of swapchain
    std::array<FGPUFenceRHIRef, 5> MeshShowFences;
    std::array<bool, 5> UsedMeshShowFences;

    FIntPoint Dimension;
    void *FrameHWImage;

    /** Duration for which the sample is valid. */
    // FTimespan TimeDuration;

    /** Sample time. */
    FMediaTimeStamp SampleTime;

    int NumSamples; // number of samples used for MSAA on rendering meshes directly

    IVulkanImpl *Impl;

    FCriticalSection AccessLock;

    static EMediaTextureSampleFormat VideoTextureFormat;
};

class FAndroidVulkanTextureSamplePool : public TMediaObjectPool<AndroidVulkanTextureSample>
{
  public:
    void Tick()
    {
        TMediaObjectPool<AndroidVulkanTextureSample>::Tick();
        for (auto wp : liveObjects)
        {
            auto pin = wp.Pin();
            if (pin.IsValid())
            {
                pin->ClearIfShown();
            }
        }
    }

    TSharedRef<AndroidVulkanTextureSample, ESPMode::ThreadSafe> AcquireShared()
    {
        auto ret = TMediaObjectPool<AndroidVulkanTextureSample>::AcquireShared();
        TWeakPtr<AndroidVulkanTextureSample, ESPMode::ThreadSafe> wp = ret;
        if (!liveObjects.Find(wp))
        {
            liveObjects.Add(wp);
        }
        return ret;
    }

    void ReleaseEverything()
    {
        for (auto wp : liveObjects)
        {
            auto pin = wp.Pin();
            if (pin.IsValid())
            {
                pin->ImplDeleted();
            }
        }

        for (auto wp : liveObjects)
        {
            auto pin = wp.Pin();
            if (pin.IsValid())
            {
                pin->ShutdownPoolable();
            }
        }
        Reset();
    }
    // for anything that has already been drawn, we want to release textures ASAP,
    // rather than whenever Unreal gets round to it.
    TArray<TWeakPtr<AndroidVulkanTextureSample, ESPMode::ThreadSafe>> liveObjects;
};
