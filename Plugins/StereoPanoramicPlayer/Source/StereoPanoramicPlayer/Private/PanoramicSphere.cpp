// Copyright 2019 UNAmedia. All rights reserved.

#include "PanoramicSphere.h"

#include "ProceduralSphereComponent.h"
#include "Components/WidgetComponent.h"
#include "PanoramicTranslucencySortPriorities.h"

#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

#define MATERIAL_INSTANCE_INITIALIZER(VarName, AssetRef) \
    static ConstructorHelpers::FObjectFinder<UMaterialInstance> AssetRef##Asset( \
        TEXT("/StereoPanoramicPlayer/Materials/Sphere/" #AssetRef "." #AssetRef)); \
    if (AssetRef ## Asset.Succeeded()) \
    { \
        VarName = UMaterialInstanceDynamic::Create(AssetRef##Asset.Object, nullptr, TEXT(#AssetRef "_instance")); \
    }

APanoramicSphere::APanoramicSphere(const FObjectInitializer& ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;

    Mesh = CreateDefaultSubobject<UProceduralSphereComponent>(TEXT("root"));
    Mesh->SetCollisionProfileName(FName(TEXT("OverlapAllDynamic")));

    // initialize translucency materials
    MATERIAL_INSTANCE_INITIALIZER(StereoExternalMaterial[0], StereoExternal)
    MATERIAL_INSTANCE_INITIALIZER(StereoNoExternalMaterial[0], StereoNoExternal)
    MATERIAL_INSTANCE_INITIALIZER(MonoExternalMaterial[0], MonoExternal)
    MATERIAL_INSTANCE_INITIALIZER(MonoNoExternalMaterial[0], MonoNoExternal)

    // initialize opaque materials
    MATERIAL_INSTANCE_INITIALIZER(StereoExternalMaterial[1], OpaqueStereoExternal)
    MATERIAL_INSTANCE_INITIALIZER(StereoNoExternalMaterial[1], OpaqueStereoNoExternal)
    MATERIAL_INSTANCE_INITIALIZER(MonoExternalMaterial[1], OpaqueMonoExternal)
    MATERIAL_INSTANCE_INITIALIZER(MonoNoExternalMaterial[1], OpaqueMonoNoExternal)

    PanoramicTexture = nullptr;
    AutoStart = false;
    MediaLayout = EPanoramicMediaLayout::StereoOverUnder;
    BlendMode = ESphereBlendMode::Translucent;

#if WITH_EDITORONLY_DATA
    HiddenInEditor = true;
#endif // WITH_EDITORONLY_DATA

    SetActorHiddenInGame(true);

    SphereConfiguration.NumOfParallels = 66;
    SphereConfiguration.NumOfMeridians = 66;
    SphereConfiguration.Radius = 1000.0f;

    SetRootComponent(Mesh);

    Mesh->TranslucencySortPriority = static_cast<int32>(EPanoramicTranslucencySortPriorities::Sphere);
}

#undef MATERIAL_INSTANCE_INITIALIZER

void APanoramicSphere::BeginPlay()
{
    Super::BeginPlay();

    if (AutoStart && PanoramicTexture != nullptr)
    {
        Play(PanoramicTexture, MediaLayout);
    }
}

FVector2D APanoramicSphere::UVMap(FVector WorldDir) const
{
    WorldDir = GetActorRotation().RotateVector(WorldDir);

    // be sure the input vector is normalized
    WorldDir.Normalize();

    // from the normalized view-vector, retrieve the longitude/latitude coords  
    float lon = atan2(WorldDir.Y, WorldDir.X);
    float lat = acos(WorldDir.Z);

    // transform to uv coords
    float u = 0.5f + lon / PI * 0.5f;
    float v = lat / PI;

    return { u, v };
}

void APanoramicSphere::AttachWidget(UWidgetComponent* Widget, const FPanoramicPosition& Pos, float Scale)
{
    float Pitch(0), Yaw(0);
    int32 Width(1), Height(1);

    switch (Pos.Units)
    {
        case EPanoramicUnits::Degrees:
        {
            Yaw = Pos.Value.X;
            Pitch = Pos.Value.Y;
            break;
        }
        case EPanoramicUnits::Pixels:
        {
            if (UTexture2D* Texture2d = Cast<UTexture2D>(PanoramicTexture))
            {
                // With some texture compression methods a texture could be resized
                // during cooking, changing the packaged texture dimensions (eg. PVR on iOS:
                // https://api.unrealengine.com/udk/Three/MobileTextureReference.html#Non-square%20textures).
                // As the values in Pos are relative to the originally imported texture sizes,
                // use GetImportedSize() instead of GetSizeX()/GetSizeY().
                const FIntPoint texSize = Texture2d->GetImportedSize();
                Width = texSize.X;
                Height = texSize.Y;
            }
            else if (UMediaTexture* MediaTexture = Cast<UMediaTexture>(PanoramicTexture))
            {
                if (UMediaPlayer* MediaPlayer = MediaTexture->GetMediaPlayer())
                {
                    const FIntPoint VideoDim = MediaPlayer->GetVideoTrackDimensions(INDEX_NONE, INDEX_NONE);
                    Width = VideoDim.X;
                    Height = VideoDim.Y;
                }
            }
            else
            {
                UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("Unable to set the widget position: %s is not a UTexture2D or a UMediaTexture."),
                    *GetFullNameSafe(PanoramicTexture));
                return;
            }

            if (Width <= 1 || Height <= 1)
            {
                UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("Unable to set the widget position: invalid texture size."));
                return;
            }

            if (MediaLayout == EPanoramicMediaLayout::StereoOverUnder)
                Height /= 2;

            //break missing is intentional
        }
        case EPanoramicUnits::UV:
        {
            float u = Pos.Value.X / Width;
            float v = Pos.Value.Y / Height;
            float lon = (u - 0.5f) * 2.f * PI;
            float lat = (v - 0.5f) * PI;

            Pitch = (lat * 180.f) / PI;
            Yaw = (lon * 180.f) / PI;
            break;
        }
        default:
            check(false); // should not happen
    }

    FRotator Rot = FRotator(Pitch, Yaw, 0.f);

    Widget->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    Widget->RegisterComponent();

    Widget->SetRelativeScale3D(FVector(Scale));

    // here -180.f is because widgets should be in front of us otherwise they will be culled
    Widget->SetRelativeLocationAndRotation(Rot.Vector() * DistanceFromWidgets(), FRotator(-Rot.Pitch, Rot.Yaw - 180.f, 0.f));
}

void APanoramicSphere::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RestoreSavedRenderSettings();

    Super::EndPlay(EndPlayReason);
}

void APanoramicSphere::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void APanoramicSphere::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // a radius of .5f would make more sense (to have a sphere of 1 unit) but with
    // a value like that the sphere would be culled by the near plane (cos we have the camera inside it)
    // default near clip plane is 10cm (see GNearClippingPlane)
    static float MinSphereRadius = GNearClippingPlane * 10.f;
    float Radius = FMath::Max(MinSphereRadius, SphereConfiguration.Radius);
    Mesh->GenerateMesh(Radius, SphereConfiguration.NumOfParallels, SphereConfiguration.NumOfMeridians);

    SetMeshMaterial();
}

#if WITH_EDITOR
void APanoramicSphere::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    if ((PropertyName == GET_MEMBER_NAME_CHECKED(APanoramicSphere, PanoramicTexture))
        || (PropertyName == GET_MEMBER_NAME_CHECKED(APanoramicSphere, MediaLayout))
        || (PropertyName == GET_MEMBER_NAME_CHECKED(APanoramicSphere, HiddenInEditor)))
    {
        SetMeshMaterial();
    }
}
#endif // WITH_EDITOR

void APanoramicSphere::Play(UTexture* NewPanoramicTexture, EPanoramicMediaLayout InMediaLayout)
{
    if (NewPanoramicTexture)
    {
        PanoramicTexture = NewPanoramicTexture;
    }

    if (!PanoramicTexture)
        return;

    MediaLayout = InMediaLayout;

    OverwriteRenderSettings();

    SetMeshMaterial();

    SetActorHiddenInGame(false);
}

void APanoramicSphere::End()
{
    RestoreSavedRenderSettings();

    SetActorHiddenInGame(true);
}

float APanoramicSphere::DistanceFromWidgets() const
{
    return SphereConfiguration.Radius * 0.9f;
}

UMaterialInstanceDynamic* APanoramicSphere::GetSuitableMaterial() const
{
    bool MediaTexture = Cast<UMediaTexture>(PanoramicTexture) != nullptr;
    const int Index = BlendMode == ESphereBlendMode::Translucent ? 0 : 1;

    if (MediaLayout == EPanoramicMediaLayout::StereoOverUnder)
    {
        if (MediaTexture)
            return StereoExternalMaterial[Index];
        else
            return StereoNoExternalMaterial[Index];
    }
    else
    {
        if (MediaTexture)
            return MonoExternalMaterial[Index];
        else
            return MonoNoExternalMaterial[Index];
    }
}

void APanoramicSphere::SetMeshMaterial()
{
    if (!Mesh)
        return;

#if WITH_EDITORONLY_DATA
    bool Hidden = HiddenInEditor && (GetWorld()->WorldType == EWorldType::Type::Editor);
    Mesh->SetVisibility(!Hidden);
#endif //WITH_EDITORONLY_DATA

    if (!PanoramicTexture)
        return;

    auto Material = GetSuitableMaterial();
    if (!Material)
        return;

    bool MediaTexture = Cast<UMediaTexture>(PanoramicTexture) != nullptr;
    FName TextureParamName = MediaTexture ? "VideoSource" : "TexSource";

    Material->SetTextureParameterValue(TextureParamName, PanoramicTexture);
    Mesh->SetMaterial(0, Material);
}

void APanoramicSphere::OverwriteRenderSettings()
{  
    if (SavedRenderSettings.Num() != 0)
        return;

    OverwriteRenderSetting(TEXT("r.ScreenPercentage"), 100);
    OverwriteRenderSetting(TEXT("r.DefaultFeature.AutoExposure"), 0);
    // see https://answers.unrealengine.com/questions/963814/425-lighting-changes-more-contrasty.html
    OverwriteRenderSetting(TEXT("r.DefaultFeature.AutoExposure.Bias"), 0);
    // see https://forums.unrealengine.com/development-discussion/rendering/22254-washed-out-colors-from-tone-mapping-how-bad-it-really-is-and-how-to-fix-it?p=1530257#post1530257
    OverwriteRenderSetting(TEXT("ShowFlag.Tonemapper"), 0);
    OverwriteRenderSetting(TEXT("r.TonemapperGamma"), 0);
    OverwriteRenderSetting(TEXT("r.Tonemapper.Quality"), 0);

#if (ENGINE_MAJOR_VERSION == 4)
    OverwriteRenderSetting(TEXT("r.PostProcessAAQuality"), 0);
    OverwriteRenderSetting(TEXT("r.TonemapperFilm"), 0);
#endif
}

void APanoramicSphere::OverwriteRenderSetting(const FString& Name, float NewValue)
{ 
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
    if (ensure(CVar))
    {
        float Value = CVar->GetFloat();
        CVar->Set(NewValue);

        SavedRenderSettings.Add(RenderSetting{ Name, Value });
    }
}

void APanoramicSphere::RestoreSavedRenderSettings()
{
    for (auto& Setting : SavedRenderSettings)
    {
        IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Setting.Key);
        check(CVar);

        CVar->Set(Setting.Value);
    }

    SavedRenderSettings.Empty();
}

void APanoramicSphere::SetBackgroundTexture(UTexture* Background)
{
    GetSuitableMaterial()->SetTextureParameterValue("BackgroundTexture", Background);
}

void APanoramicSphere::SetCutOffTexture(UTexture* CutOffTexture)
{
    GetSuitableMaterial()->SetTextureParameterValue("CutOffTexture", CutOffTexture);
}

void APanoramicSphere::SetFadeIntensity(float Intensity)
{
    GetSuitableMaterial()->SetScalarParameterValue("Fade", Intensity);
}

void APanoramicSphere::SetCutOffAlpha(float CutOffAlpha)
{
    GetSuitableMaterial()->SetScalarParameterValue("CutOffAlpha", CutOffAlpha);
}

void APanoramicSphere::SetCutOffIntensity(float Intensity)
{
    GetSuitableMaterial()->SetScalarParameterValue("CutOff", Intensity);
}
