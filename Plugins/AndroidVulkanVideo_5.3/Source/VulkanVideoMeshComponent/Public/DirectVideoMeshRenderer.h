// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Attaches to a static mesh, and allows the vertex data
// to be passed into the vulkanimpl for rendering
// ------------------------------------------------
#pragma once

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "IMediaEventSink.h"
#include "MediaObjectPool.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaSampleSink.h"

#include "Logging/StructuredLog.h"

#include "IVulkanVertexData.h"

#include "RHIDefinitions.h"
#include "RendererInterface.h"

#include "DirectVideoMeshRenderer.generated.h"

class AndroidVulkanTextureSample;

class FRendererSceneViewExt;

class FRawStaticIndexBuffer;
struct FStaticMeshVertexBuffers;

DECLARE_LOG_CATEGORY_EXTERN(LogDirectVideoMeshRenderer, Log, All);

UENUM()
enum EStereoMode
{
    STEREO_NONE UMETA(DisplayName = "None"),
    STEREO_SBS UMETA(DisplayName = "Side By Side"),
    STEREO_TB UMETA(DisplayName = "Top and Bottom")
};

UENUM()
enum EMeshCullMode
{
    MESH_CM_NONE = 0 UMETA(DisplayName = "None"),
    MESH_CM_CCW = 1 UMETA(DisplayName = "Counter Clockwise / Cull back faces"),
    MESH_CM_CW = 2 UMETA(DisplayName = "Clockwise / Cull front faces")
};

UCLASS(Blueprintable, ClassGroup = (Rendering, Common),
       hidecategories = (Object, Activation, "Components|Activation"), ShowCategories = (Mobility),
       editinlinenew, meta = (BlueprintSpawnableComponent))
class VULKANVIDEOMESHCOMPONENT_API UDirectVideoMeshRendererComponent : public UActorComponent
{
    friend class FRendererSceneViewExt;
    GENERATED_BODY()
  public:
    UDirectVideoMeshRendererComponent();
    ~UDirectVideoMeshRendererComponent();

    void HandlePlayerMediaEvent(EMediaSampleSinkEvent Event);

    void DeInit();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|DirectVideo")
    TEnumAsByte<EStereoMode> StereoMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|DirectVideo")
    UMediaPlayer *LinkedPlayer;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|DirectVideo")
    bool HideMeshWhileWeHaveVideo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components|DirectVideo")
    TEnumAsByte<EMeshCullMode> CullMode;

    // FMediaTextureSampleSink (called indirectly from _TextureSink)
    virtual bool Enqueue(const TSharedRef<IMediaTextureSample, ESPMode::ThreadSafe> &Sample);
    virtual int32 Num() const
    {
        if (CurrentSample.IsValid())
            return 1;
        return 0;
    }
    virtual void RequestFlush();

    virtual void BeginDestroy() override;

  protected:
    // draw to a command list (which is already in a render pass with outputs set up)
    void DrawToCommandList(FRHICommandList &RHICmdList, FSceneView &InView);
    // load mesh, update matrices, create vulkan images (must be outside render pass)
    void DoWorkOutsideRenderPass(FRHICommandList &RHICmdList, FSceneViewFamily &InViewFamily);

    void UpdateVisibility();

    class _TextureSink : public FMediaTextureSampleSink
    {
      public:
        _TextureSink(UDirectVideoMeshRendererComponent *InOwner)
        {
            Owner = InOwner;
            FlushCount = 0;

            EventDelegate =
                OnMediaSampleSinkEvent().AddRaw(this, &_TextureSink::ReceiveEventHandler);
        }

        virtual ~_TextureSink()
        {

            OnMediaSampleSinkEvent().Remove(EventDelegate);
        }
        virtual bool Enqueue(
            const TSharedRef<IMediaTextureSample, ESPMode::ThreadSafe> &Sample) override
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, Verbose, "Enqueue");
            auto POwner = Owner.Get();
            if (POwner != NULL)
            {
                UE_LOGFMT(LogDirectVideoMeshRenderer, Verbose, "Enqueue2");
                return POwner->Enqueue(Sample);
            }
            return false;
        }
        virtual int32 Num() const override
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, Verbose, "Num");
            auto POwner = Owner.Get();
            if (POwner != NULL)
            {
                UE_LOGFMT(LogDirectVideoMeshRenderer, Verbose, "Num2");
                return POwner->Num();
            }
            return 0;
        }
        virtual void RequestFlush() override
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, Verbose, "Flush");

            FlushCount += 1;
            auto POwner = Owner.Get();
            if (POwner != NULL)
            {
                UE_LOGFMT(LogDirectVideoMeshRenderer, Verbose, "Flush2");
                POwner->RequestFlush();
            }
        }

        void ReceiveEventHandler(EMediaSampleSinkEvent Event, const FMediaSampleSinkEventData &Data)
        {
            int iEvent = (int)Event;
            UE_LOGFMT(LogDirectVideoMeshRenderer, Verbose, "On event:{1}", iEvent);
            auto POwner = Owner.Get();
            if (POwner != NULL)
            {
                POwner->HandlePlayerMediaEvent(Event);
            }
        }

        TWeakObjectPtr<UDirectVideoMeshRendererComponent> Owner;
        int FlushCount;

        FDelegateHandle EventDelegate;
    };

    void Initialize();

    void GetMeshObject();
    bool ConstructMeshFromRenderData(const FStaticMeshVertexBuffers &VertexBuffers,
                                     const FRawStaticIndexBuffer &IndexBuffer);
    void GetShaderInfo(const FSceneView *InView, float *m44, float *vp44,
                       float *pre_view_translation);
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction *ThisTickFunction) override;

    virtual void Serialize(FArchive &Ar) override;

    UPROPERTY()
    bool HasMesh;
    // raw mesh data as passed to RenderToMesh
    // we save this in UObject::Serialize so that
    // we don't need to load it from the mesh at
    // runtime
    UPROPERTY()
    TArray<float> PositionsAsFloats; // x,y,z,u,v (floats)

    // vertexData is not a UStruct because we pass it through to
    // native android code
    TArray<VertexData> Positions; // x,y,z,u,v (floats)

    UPROPERTY()
    TArray<uint32> Indices; // index buffer for mesh (uint32)

    TSharedPtr<_TextureSink> TextureSink;

    // sample being initialized to render (this can be slow sometimes, so we do it before swapping
    // frames)
    TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> CurrentSample;
    // the currently prepared sample which can just be rendered each frame as
    // we know it is prepared already
    TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> PreparedSample;

    TWeakPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> Facade;

    TObjectPtr<AStaticMeshActor> MeshActor;

    TSharedPtr<FRendererSceneViewExt, ESPMode::ThreadSafe> ExtensionHolder;

    bool Initialized;
};
