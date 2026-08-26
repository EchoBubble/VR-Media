// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Renders mesh directly to framebuffer from video
// stream (without texture between)
// ------------------------------------------------
#include "DirectVideoMeshRenderer.h"

#if PLATFORM_ANDROID
#include "AndroidVulkanTextureSample.h"
#endif

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineModule.h"
#include "IMediaPlayer.h"
#include "RHIResources.h"
#include "RawIndexBuffer.h"
#include "RenderGraphBuilder.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "StaticMeshResources.h"
#include "StereoRendering.h"

#include "Math/Matrix.h"

#include "MeshDescription.h"

DEFINE_LOG_CATEGORY(LogDirectVideoMeshRenderer);

static FGuid OurGUID(0x9bf2d7c6, 0xb2b84d26, 0xb6ae5a3a, 0xc9883569);

class FRendererSceneViewExt : public FSceneViewExtensionBase
{
  public:
    FRendererSceneViewExt(const FAutoRegister &AutoRegister,
                          UDirectVideoMeshRendererComponent &InOwner)
        : FSceneViewExtensionBase(AutoRegister), Owner(InOwner)
    {
    }

    virtual void PostRenderView_RenderThread(FRDGBuilder &GraphBuilder, FSceneView &InView) override
    {
    }

    virtual void PostRenderBasePassMobile_RenderThread(FRHICommandList &RHICmdList,
                                                       FSceneView &InView) override

    {
        UE_LOGFMT(LogDirectVideoMeshRenderer, VeryVerbose, "PostRenderBasePassMobile");

#if PLATFORM_ANDROID
        Owner.DrawToCommandList(RHICmdList, InView);
#endif
    }

    virtual void SetupViewFamily(FSceneViewFamily &InViewFamily) override
    {
    }

    virtual void SetupView(FSceneViewFamily &InViewFamily, FSceneView &InView) override
    {
    }

    virtual void PreRenderViewFamily_RenderThread(FRDGBuilder &GraphBuilder,
                                                  FSceneViewFamily &InViewFamily)
    {
#if PLATFORM_ANDROID

        // we are outside of a command list, so we can update the mesh if needed
        UE_LOGFMT(LogDirectVideoMeshRenderer, VeryVerbose, "PreRenderViewFamily_RenderThread");
        Owner.DoWorkOutsideRenderPass(GraphBuilder.RHICmdList, InViewFamily);
#endif
    }

    virtual void PreRenderView_RenderThread(FRDGBuilder &GraphBuilder, FSceneView &InView)
    {
    }

    virtual void BeginRenderViewFamily(FSceneViewFamily &InViewFamily) override
    {
        Owner.UpdateVisibility();
    }

    UDirectVideoMeshRendererComponent &Owner;
};

UDirectVideoMeshRendererComponent::UDirectVideoMeshRendererComponent()
{
    HideMeshWhileWeHaveVideo = true;
    PrimaryComponentTick.bCanEverTick = true;
    Initialized = false;
    StereoMode = EStereoMode::STEREO_NONE;
    CullMode = EMeshCullMode::MESH_CM_CCW;
}

UDirectVideoMeshRendererComponent::~UDirectVideoMeshRendererComponent()
{
    DeInit();
}

void UDirectVideoMeshRendererComponent::DeInit()
{
    Initialized = false;
#if PLATFORM_ANDROID
    UE_LOGFMT(LogDirectVideoMeshRenderer, VeryVerbose, "Deinit");
    // Extensions clean up on destroy
    ExtensionHolder.Reset();

    if (PreparedSample != NULL)
    {
        AndroidVulkanTextureSample *VulkanSample =
            static_cast<AndroidVulkanTextureSample *>(PreparedSample.Get());
        VulkanSample->ImplDeleted();
    }
    if (CurrentSample != NULL)
    {
        AndroidVulkanTextureSample *VulkanSample =
            static_cast<AndroidVulkanTextureSample *>(CurrentSample.Get());
        VulkanSample->ImplDeleted();
    }
    CurrentSample = NULL;
    PreparedSample = NULL;
    if (Facade != NULL)
    {
        auto PFacade = Facade.Pin();
    }
#endif

    Facade.Reset();
    // the media facade sample sink keeps a weak pointer to
    // the texture sink and then clears it if destroyed,
    // so we don't need to remove the texture sink, we can just
    // kill it here.
    TextureSink.Reset();
}

void UDirectVideoMeshRendererComponent::BeginDestroy()
{
    DeInit();
    UActorComponent::BeginDestroy();
}

void UDirectVideoMeshRendererComponent::Initialize()
{
#if PLATFORM_ANDROID
    TextureSink = MakeShared<_TextureSink>(this);

    if (Facade == NULL)
    {
        if (LinkedPlayer != NULL)
        {
            Facade = LinkedPlayer->GetPlayerFacade();
        }
    }

    if (Facade != NULL)
    {
        auto PFacade = Facade.Pin();
        auto Player = PFacade->GetPlayer();
        if (Player.IsValid() && Player->GetPlayerPluginGUID() == OurGUID)
        {
            TSharedRef<FMediaTextureSampleSink> sinkPtr = TextureSink.ToSharedRef();
            PFacade.Get()->AddVideoSampleSink(sinkPtr);

            UE_LOGFMT(LogDirectVideoMeshRenderer, VeryVerbose, "Registered for frames");
            Initialized = true;
        }
    }
    else
    {
        UE_LOGFMT(LogDirectVideoMeshRenderer, VeryVerbose, "No media player");
    }

    GetMeshObject();

    if (ExtensionHolder == NULL)
    {
        ExtensionHolder = FSceneViewExtensions::NewExtension<FRendererSceneViewExt>(*this);
    }

#endif
}

void UDirectVideoMeshRendererComponent::HandlePlayerMediaEvent(EMediaSampleSinkEvent Event)
{
    int IEvent = (int)Event;
    UE_LOGFMT(LogDirectVideoMeshRenderer, VeryVerbose, "Handle event {0}", IEvent);

    switch (Event)
    {
    case EMediaSampleSinkEvent::Detached:
    case EMediaSampleSinkEvent::MediaClosed:
        CurrentSample = NULL;
        PreparedSample = NULL;
        break;
    }
}

void UDirectVideoMeshRendererComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                      FActorComponentTickFunction *ThisTickFunction)
{
    if (!Initialized)
    {
        Initialize();
    }
}

void UDirectVideoMeshRendererComponent::UpdateVisibility()
{
    if (HideMeshWhileWeHaveVideo)
    {
        MeshActor = GetOwner<AStaticMeshActor>();
        if (MeshActor == NULL)
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, Warning,
                      "Mesh renderer has to be on StaticMeshActor");
            return;
        }
        // need to unhide or hide the underlying staticmesh here
        // depending on whether we have video
        UStaticMeshComponent *MeshComponent = MeshActor->GetStaticMeshComponent();
        if (MeshComponent == NULL)
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, Warning,
                      "Mesh renderer needs a static mesh component");
            return;
        }

        MeshComponent->bRenderInDepthPass = (PreparedSample == NULL);
        MeshComponent->SetRenderInMainPass(PreparedSample == NULL);
    }
}

void UDirectVideoMeshRendererComponent::GetMeshObject()
{
    MeshActor = GetOwner<AStaticMeshActor>();
    if (MeshActor == NULL)
    {
        UE_LOGFMT(LogDirectVideoMeshRenderer, Warning,
                  "Mesh renderer has to be on StaticMeshActor");
        return;
    }
    UStaticMeshComponent *MeshComponent = MeshActor->GetStaticMeshComponent();
    TObjectPtr<UStaticMesh> Mesh = MeshComponent->GetStaticMesh();
#if WITH_EDITOR
    if (Mesh != NULL)
    {
        FStaticMeshRenderData *RenderData = Mesh->GetRenderData();
        if (RenderData != NULL)
        {
            const FStaticMeshLODResources &LODRenderData = RenderData->LODResources[0];
            const auto &VertexBuffers = LODRenderData.VertexBuffers;
            const auto &IndexBuffer = LODRenderData.IndexBuffer; // index buffer for rendering this

            if (ConstructMeshFromRenderData(VertexBuffers, IndexBuffer))
            {
                HasMesh = true;
            }
            else
            {
                UE_LOGFMT(LogDirectVideoMeshRenderer, Warning, "Mesh making failed");
            }
        }
        else
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, Warning, "No render data");
        }
    }
    else
    {
        UE_LOGFMT(LogDirectVideoMeshRenderer, Warning, "Mesh is null");
    }
#endif
}

void DumpMatrix(const char *Name, FMatrix44f Matrix)
{
    char MatrixDesc[256];
    int ofs = snprintf(MatrixDesc, 250, "%s:\n", Name);
    for (int c = 0; c < 4; c++)
    {
        for (int d = 0; d < 4; d++)
        {
            int len = snprintf(&MatrixDesc[ofs], 250 - ofs, "%4.4f,", Matrix.M[c][d]);
            ofs += len;
        }
        MatrixDesc[ofs] = '\n';
        ofs++;
    }
    MatrixDesc[ofs] = 0;
    UE_LOGFMT(LogDirectVideoMeshRenderer, Verbose, "{0}", MatrixDesc);
}

void UDirectVideoMeshRendererComponent::DrawToCommandList(FRHICommandList &RHICmdList,
                                                          FSceneView &InView)
{
#if PLATFORM_ANDROID
    if (!Initialized)
    {
        return;
    }
    if (PreparedSample != NULL)
    {
        AndroidVulkanTextureSample *VulkanSample =
            static_cast<AndroidVulkanTextureSample *>(PreparedSample.Get());

        UE_LOG(LogDirectVideoMeshRenderer, VeryVerbose,
               TEXT("Draw to command list Immediate:%d Prepared Sample:%p"),
               RHICmdList.IsImmediate(), VulkanSample->GetHWImage());
        const FTextureRHIRef &ColourTex = InView.Family->RenderTarget->GetRenderTargetTexture();
        VulkanSample->RenderToMesh(ColourTex, RHICmdList, (void *)(MeshActor.Get()));
    }
    else
    {
        if (Facade.IsValid())
        {
            auto PFacade = Facade.Pin();
            auto Player = PFacade->GetPlayer();
            if (Player.IsValid() && Player->GetPlayerPluginGUID() == OurGUID)
            {
                AndroidVulkanTextureSample::CaptureRenderPass(Player.Get(), RHICmdList);
            }
        }
    }
#endif
}

void UDirectVideoMeshRendererComponent::GetShaderInfo(const FSceneView *InView, float *m44,
                                                      float *vp44, float *pre_view_translation)
{
    FTransform Transform = MeshActor->ActorToWorld();
    FMatrix44f LocalToRelativeWorld = FMatrix44f(Transform.ToMatrixWithScale());
    //    DumpMatrix("Model", LocalToRelativeWorld);

    auto TranslatedWorldToClip =
        FMatrix44f(InView->ViewMatrices.GetTranslatedViewProjectionMatrix());
    //            DumpMatrix("VP", TranslatedWorldToClip);
    auto MVPMatrix = LocalToRelativeWorld * TranslatedWorldToClip;
    //            DumpMatrix("MVP", MVPMatrix);

    auto Translation = InView->ViewMatrices.GetPreViewTranslation();
    FVector4f Translation4 = FVector4f(Translation.X, Translation.Y, Translation.Z, 0);
    UE_LOGFMT(LogDirectVideoMeshRenderer, VeryVerbose, "Pre view translation 2:{0},{1},{2},{3}",
              Translation4.X, Translation4.Y, Translation4.Z, Translation4.W);
    memcpy(vp44, TranslatedWorldToClip.M, 16 * sizeof(float));
    // auto TransposedVP=TranslatedWorldToClip.GetTransposed();
    //            memcpy(vp44,TransposedVP.M,16*sizeof(float));
    //            auto TransposedM = LocalToRelativeWorld.GetTransposed();
    //            memcpy(m44,TransposedM.M,16*sizeof(float));
    memcpy(m44, LocalToRelativeWorld.M, 16 * sizeof(float));
    pre_view_translation[0] = Translation4.X;
    pre_view_translation[1] = Translation4.Y;
    pre_view_translation[2] = Translation4.Z;
    pre_view_translation[3] = Translation4.W;
}

void UDirectVideoMeshRendererComponent::DoWorkOutsideRenderPass(FRHICommandList &RHICmdList,
                                                                FSceneViewFamily &InViewFamily)
{
#if PLATFORM_ANDROID
    if (!Initialized)
    {
        return;
    }
    int ImplStereoMode = 0;
    switch (StereoMode)
    {
    case EStereoMode::STEREO_SBS:
        ImplStereoMode = 1;
        break;
    case EStereoMode::STEREO_TB:
        ImplStereoMode = 2;
        break;
    };
    const FTextureRHIRef &ColourTex = InViewFamily.RenderTarget->GetRenderTargetTexture();

    // update view and mesh based on whatever sample is loaded as that
    // is independent of sample image preparation
    if (PreparedSample != NULL || CurrentSample != NULL)
    {
        ShaderViewMatrices ViewMatrices;
        ShaderModelMatrix ModelMatrix;
        AndroidVulkanTextureSample *VulkanPrepared =
            static_cast<AndroidVulkanTextureSample *>(PreparedSample.Get());
        if (VulkanPrepared == NULL)
        {
            VulkanPrepared = static_cast<AndroidVulkanTextureSample *>(CurrentSample.Get());
        }

        UE_LOG(LogDirectVideoMeshRenderer, VeryVerbose,
               TEXT("Work outside render pass Immediate:%d CurrentSample:%p"),
               RHICmdList.IsImmediate(), VulkanPrepared->GetHWImage());
        for (auto &InView : InViewFamily.Views)
        {

            if (IStereoRendering::IsASecondaryView(*InView))
            {
                GetShaderInfo(InView, ModelMatrix.m44, ViewMatrices.vp44_2,
                              ViewMatrices.pre_view_translation_2);
            }
            else
            {
                GetShaderInfo(InView, ModelMatrix.m44, ViewMatrices.vp44,
                              ViewMatrices.pre_view_translation);
            }
        }
        VulkanPrepared->UpdateMesh(ColourTex, RHICmdList, Positions, Indices, ModelMatrix,
                                   (void *)(MeshActor.Get()));
        VulkanPrepared->UpdateViewMatrices(ColourTex, RHICmdList, ViewMatrices, ModelMatrix,
                                           (void *)(MeshActor.Get()));
    }
    // if we have a current sample, we need to prepare it for rendering
    if (CurrentSample != NULL)
    {
        auto VulkanCurrent = static_cast<AndroidVulkanTextureSample *>(CurrentSample.Get());
        auto state = VulkanCurrent->GetRenderState();
        UE_LOG(LogDirectVideoMeshRenderer, VeryVerbose,
               TEXT("Current Sample handling State:%d CurrentSample:%p"), state,
               VulkanCurrent->GetHWImage());
        switch (state)
        {
        case AndroidVulkanTextureSample::RenderState::NONE:
            VulkanCurrent->InitFrameForMeshRendering(ColourTex, RHICmdList,
                                                     (void *)(MeshActor.Get()), ImplStereoMode);
            return;
        case AndroidVulkanTextureSample::RenderState::INIT:
            // do nothing, still waiting for init
            break;
        }
        // n.b. initframeformeshrendering will set the state to READY
        // if it can immediately be used (at which point it will transition to shown state)
        if (state == AndroidVulkanTextureSample::RenderState::READY)
        {
            // we can use this sample now
            PreparedSample = CurrentSample;
            CurrentSample.Reset();
            UE_LOG(LogDirectVideoMeshRenderer, VeryVerbose,
                   TEXT("Immediately using current sample %p"),
                   static_cast<AndroidVulkanTextureSample *>(PreparedSample.Get())->GetHWImage());
        }
    }

#endif
}

bool UDirectVideoMeshRendererComponent::Enqueue(
    const TSharedRef<IMediaTextureSample, ESPMode::ThreadSafe> &Sample)
{
    if (!Initialized)
    {
        return false;
    }
    auto PFacade = Facade.Pin();
#if PLATFORM_ANDROID
    if (PFacade->GetPlayer()->GetPlayerPluginGUID() == OurGUID)
    {
        void *oldImgPtr = NULL;
        if (CurrentSample.IsValid())
        {
            AndroidVulkanTextureSample *VulkanSample2 =
                static_cast<AndroidVulkanTextureSample *>(CurrentSample.Get());
            oldImgPtr = VulkanSample2->GetHWImage();
        }
        CurrentSample = Sample;
        AndroidVulkanTextureSample *VulkanSample =
            static_cast<AndroidVulkanTextureSample *>(CurrentSample.Get());
        void *ImgPtr = VulkanSample->GetHWImage();

        void *preparedImgPtr = NULL;
        if (PreparedSample.IsValid())
        {
            AndroidVulkanTextureSample *VulkanPrepared =
                static_cast<AndroidVulkanTextureSample *>(PreparedSample.Get());
            preparedImgPtr = VulkanPrepared->GetHWImage();
        }

        if (VulkanSample != NULL)
        {
            switch (CullMode)
            {
            case EMeshCullMode::MESH_CM_CCW:
                VulkanSample->SetMeshCullMode(ERasterizerCullMode::CM_CCW);
                break;
            case EMeshCullMode::MESH_CM_CW:
                VulkanSample->SetMeshCullMode(ERasterizerCullMode::CM_CW);
                break;
            case EMeshCullMode::MESH_CM_NONE:
                VulkanSample->SetMeshCullMode(ERasterizerCullMode::CM_None);
                break;
            }
        }

        UE_LOG(LogDirectVideoMeshRenderer, VeryVerbose,
               TEXT("Mesh component has sample image:%p old:%p prepared:%p"), ImgPtr, oldImgPtr,
               preparedImgPtr);
    }
    else
    {
        UE_LOGFMT(LogDirectVideoMeshRenderer, Warning, "Mesh component has wrong player {0}",
                  PFacade.Get()->GetGuid());
    }
#endif
    return true;
}

void UDirectVideoMeshRendererComponent::Serialize(FArchive &Ar)
{
#if WITH_EDITOR
    if (Ar.IsSaving())
    {
        if (!HasMesh)
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, Warning, "Needs mesh object");

            GetMeshObject();
        }
        if (!HasMesh)
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, Warning,
                      "Couldn't get mesh for DirectVideoMeshRenderComponent");
        }
        else
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, VeryVerbose, "Saving mesh details");
        }
    }
#endif
    UActorComponent::Serialize(Ar);

    if (Ar.IsLoading())
    {
        if (HasMesh)
        {
            int NumVertices = PositionsAsFloats.Num() / 5;
            Positions.Init({}, NumVertices);
            for (int c = 0; c < NumVertices; c++)
            {
                Positions[c].x = PositionsAsFloats[c * 5];
                Positions[c].y = PositionsAsFloats[c * 5 + 1];
                Positions[c].z = PositionsAsFloats[c * 5 + 2];
                Positions[c].u = PositionsAsFloats[c * 5 + 3];
                Positions[c].v = PositionsAsFloats[c * 5 + 4];
            }
            UE_LOGFMT(LogDirectVideoMeshRenderer, VeryVerbose,
                      "DirectVideoMeshRenderComponent - Vertices {0} Indices {1}", Indices.Num(),
                      Positions.Num());
        }
        else
        {
            UE_LOGFMT(LogDirectVideoMeshRenderer, Warning,
                      "No saved mesh for DirectVideoMeshRenderComponent");
        }
    }
}

bool UDirectVideoMeshRendererComponent::ConstructMeshFromRenderData(
    const FStaticMeshVertexBuffers &VertexBuffers, const FRawStaticIndexBuffer &IndexBuffer)
{
    const FPositionVertexBuffer &Pos =
        VertexBuffers.PositionVertexBuffer; // contains vertex positions
    const FStaticMeshVertexBuffer &TangentsAndTextureCoords =
        VertexBuffers
            .StaticMeshVertexBuffer; // contains tangents, texture coords and lightmap coords
    const FColorVertexBuffer &Color = VertexBuffers.ColorVertexBuffer; // vertex colours
    if (IndexBuffer.GetIndexDataSize() == 0)
    {
        UE_LOGFMT(LogDirectVideoMeshRenderer, Warning, "No indices for mesh");
        return false;
    }

    // make 1 buffer with:
    //   position (float * 3, packed as 4 byte)
    //   texture coordinates (float*2), packed as 4 byte
    int NumVertices = Pos.GetNumVertices();
    Positions.Init({}, NumVertices);
    int NumIndices = IndexBuffer.GetNumIndices();
    Indices.Init({}, NumIndices);

    for (int c = 0; c < NumVertices; c++)
    {
        const FVector3f &VPos = Pos.VertexPosition(c);
        Positions[c].x = VPos.X;
        Positions[c].y = VPos.Y;
        Positions[c].z = VPos.Z;
        FVector2f TexPos = TangentsAndTextureCoords.GetVertexUV(c, 0);
        Positions[c].u =  1.0f - TexPos.X;;
        Positions[c].v = TexPos.Y;
    }

    // and 1 index buffer (int32)
    IndexBuffer.GetCopy(Indices);
    PositionsAsFloats.Init({}, NumVertices * 5);
    for (int c = 0; c < NumVertices; c++)
    {
        PositionsAsFloats[c * 5] = Positions[c].x;
        PositionsAsFloats[c * 5 + 1] = Positions[c].y;
        PositionsAsFloats[c * 5 + 2] = Positions[c].z;
        PositionsAsFloats[c * 5 + 3] = Positions[c].u;
        PositionsAsFloats[c * 5 + 4] = Positions[c].v;
    }
    return true;
}

void UDirectVideoMeshRendererComponent::RequestFlush()
{
    if (!Initialized)
    {
        return;
    }
    UE_LOGFMT(LogDirectVideoMeshRenderer, Verbose, "RequestFlush");
    CurrentSample.Reset();
    // A source switch invalidates the native AImage held by the prepared
    // sample as well. Keeping it here lets the render thread submit draws
    // against an image that Impl->close() has already released.
    PreparedSample.Reset();
}
