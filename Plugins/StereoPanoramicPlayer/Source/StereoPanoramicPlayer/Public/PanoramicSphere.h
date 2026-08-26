// Copyright 2019 UNAmedia. All rights reserved.
/** @file */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "MediaTexture.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "PanoramicMediaLayout.h"
#include "PanoramicPosition.h"

#include "PanoramicSphere.generated.h"

/** 
 *  Configuration for the sphere generation.
 *  The result number of triangles will be equal to
 *  ((NumOfParallels - 1) * (NumOfMeridians - 1) * 2)
 */
USTRUCT(BlueprintType)
struct STEREOPANORAMICPLAYER_API FSphereConfig
{
    GENERATED_BODY()

    /** Num of parallels */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    int32 NumOfParallels = 66;

    /** Num of meridians */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    int32 NumOfMeridians = 66;

    /** The radius of the sphere in unreal units; Irrelevant for translucent blend mode. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    float Radius = 1000;
};

/**
 * The blending rendering mode of the panoramic sphere.
 */
UENUM(BlueprintType)
enum class ESphereBlendMode : uint8
{
    /** The rendering of the sphere is based on the UE4 translucent rendering pipeline.
     * More expensive to render, but the sphere can be drawn over any existing 3D scene (hiding it).
     * Rendering of 3D objects inside the sphere requires a special setup (see @ref howto_render3dInside).
     * It's the default mode. */
    Translucent UMETA(DisplayName = "Translucent blend mode"),
    /** The rendering of the sphere is based on the UE4 opaque rendering pipeline.
     * More efficient to render, but the surrounding 3D scene could cause visual artifacts
     * and rendered inside the sphere if not explicitly avoided.
     * Rendering of 3D objects inside the sphere is supported out-of-the-box.
     * See @ref ps_blendMode. */
    Opaque      UMETA(DisplayName = "Opaque blend mode")
};

/**
 * PanoramicSphere is the main class to view a panoramic source 
 * (both UTexture2D and MediaTexture).
 * The source is rendered through a procedural generated sphere mesh.
 * It can be configured through FSphereConfig
 *
 * @see UProceduralSphereComponent
 * @see FSphereConfig
 */
UCLASS(BlueprintType, Config = Game)
class STEREOPANORAMICPLAYER_API APanoramicSphere : public AActor
{
    GENERATED_UCLASS_BODY()

public:

    //~ AActor interface

    /** Override AActor::BeginPlay */
    virtual void BeginPlay() override;

    /** Override AActor::EndPlay */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Override AActor::Tick */
    virtual void Tick(float DeltaSeconds) override;

    /** Override AActor::OnConstruction */
    virtual void OnConstruction(const FTransform& Transform) override;

    //~ End AActor interface

#if WITH_EDITOR
    /** Override AActor::PostEditChangeProperty */
    void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
#endif // WITH_EDITOR

public:

    /** 
     * Enters into panoramic mode playing the given UTexture 
     * @param Media the media to play
     * @param MediaLayout the media layout (@see EPanoramicMediaLayout)
     */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer")
    void Play(UTexture* Media, EPanoramicMediaLayout MediaLayout);

    /** Exits from panoramic mode */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer")
    void End();

public:

    /** Map the input unit vector (in world space) to its UV coordinates */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Interaction")
    FVector2D UVMap(FVector WorldDir) const;

public:

    /** 
     * Attach the given widget component to the panoramic sphere as its child.
     * @param Widget the widget to be attached.
     * @param Pos where to place the widget @see FPanoramicPosition.
     * @param Scale scale factor to zoom in/out the widget.
     */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Widgets")
    void AttachWidget(class UWidgetComponent* Widget, const FPanoramicPosition& Pos, float Scale = 0.1f);

public:

    // material params editing functions

    /** Sets the background texture. 
        This texture is used to crossfade during the fade in/out. */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Material")
    void SetBackgroundTexture(UTexture* Background);

    /** Sets the cutoff texture. When the cutoff is enabled, the texture texels
        are compared with the cutoff intensity. When the texel is below the cutoff 
        threshold, it will be discarded (not rendered). */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Material")
    void SetCutOffTexture(UTexture* CutOffTexture);

    /** Sets the fade intensity. The value must be in [0,1]. */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Material")
    void SetFadeIntensity(float Intensity);

    /** Sets the cutoff alpha. The value must be in [0,1]. */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Material")
    void SetCutOffAlpha(float CutOffAlpha);

    /** Sets the cutoff intensity. The value must be in [0,1]. */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Material")
    void SetCutOffIntensity(float Intensity);

public:

    /** The Texture to display. 
    Can be a Utexture2D or even a UMediaTexture but in this case
    you have to handle the playback by hand externally. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    UTexture* PanoramicTexture;

    /** The media layout */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    EPanoramicMediaLayout MediaLayout;

    /** The blend mode */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    ESphereBlendMode BlendMode;

    /** When true it plays the specified PanoramicTexture automatically */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    bool AutoStart;

    /** Defines how the sphere should be generated */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    FSphereConfig SphereConfiguration;

#if WITH_EDITORONLY_DATA
    /** Flag to toggle the mesh visibility in editor */
    UPROPERTY(Transient, EditInstanceOnly, Category = "StereoPanoramicPlayer")
    bool HiddenInEditor;
#endif // WITH_EDITORONLY_DATA

public:

    /** Returns the distance from the sphere to where all widgets are placed regarding their centers */
	UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Widgets")
    float DistanceFromWidgets() const;

private:

    /** Returns the suitable material based
    on the texture type and the MediaLayout */
    UMaterialInstanceDynamic* GetSuitableMaterial() const;

    // Set the proper material to the sphere mesh
    void SetMeshMaterial();

    // functions to overwrite/restore the render settings
    void OverwriteRenderSettings();
    void OverwriteRenderSetting(const FString& Name, float NewValue);
    void RestoreSavedRenderSettings();

private:

    /** The mesh generated procedurally. 
    You can customize the generation editing
    the sphere configuration @see FSphereConfig*/
    UPROPERTY()
    class UProceduralSphereComponent* Mesh;

    /** The material instance used for rendering when the MediaLayout is
    stereo and the texture is a UMediaTexture. (translucency & opaque blend mode) */
    UPROPERTY(Transient)
    UMaterialInstanceDynamic*   StereoExternalMaterial[2];

    /** The material instance used for rendering when the MediaLayout is
    stereo and the texture is a UTexture2D. (translucency & opaque blend mode) */
    UPROPERTY(Transient)
    UMaterialInstanceDynamic*   StereoNoExternalMaterial[2];

    /** The material instance used for rendering when the MediaLayout is
    stereo and the texture is a UMediaTexture. (translucency & opaque blend mode) */
    UPROPERTY(Transient)
    UMaterialInstanceDynamic*   MonoExternalMaterial[2];

    /** The material instance used for rendering when the MediaLayout is
    mono and the texture is a UTexture2D. (translucency & opaque blend mode) */
    UPROPERTY(Transient)
    UMaterialInstanceDynamic*   MonoNoExternalMaterial[2];

    using RenderSetting = TPair<FString, float>;

    /** Render settings saved when we enter in panoramic mode
    and to be restored when we exit */
    TArray<RenderSetting> SavedRenderSettings;
};
