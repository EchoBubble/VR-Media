// Copyright UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderCommandFence.h"

#include "NavigationTexture.generated.h"

class UTexture2D;

/**
 * The navigation texture "wraps" a UTexture2D and allow the access of its raw data from the cpu.
 * A navigation texture is used by the panoramic director to navigate between stages.
 * @see UPanoramicStage::NavigationLookupTexture and FPanoramicStageNavigation::LookupColor for additional details.
 */
UCLASS()
class STEREOPANORAMICPLAYER_API UNavigationTexture : public UObject
{
    GENERATED_BODY()

public:

    /**
     *  Create an UNavigationTexture instance from a UTexture2D (the ***Compression Settings*** of your texture must be set to 'UserInterface2D (RGBA)'!).
     *  @param  Outer The outer of the new instance
     *  @param  InTexture The input texture
     *  @return the new instance if the texture is not nullptr and has a valid pixel format, nullptr otherwise.
     */
    static UNavigationTexture* MakeFromTexture(UObject* Outer, UTexture2D* InTexture);

    /**
     * @brief Returns the texel color at the given UV coordinates.
     * 
     * @param[in] UV Input UV coordinates (each coordinate in [0, 1]).
     * @param[out] OutColor The returned texel color at the provided UV coordinates. Valid only if the method returns `true`.
     * @return true if the call succeeded and OutColor is valid, false otherwise.
     */
    bool Lookup(const FVector2D& UV, FColor& OutColor);

public:

    //~ Begin UObject
    /// Overriden UObject::BeginDestroy().
    virtual void BeginDestroy() override;
    /// Overriden UObject::IsReadyForFinishDestroy().
    virtual bool IsReadyForFinishDestroy() override;
    //~ End UObject

private:

    UNavigationTexture();

    bool InitFromTexture(UTexture2D* InTexture);

private:

    TWeakObjectPtr<UTexture2D> SrcTexture;
    int32 TextureWidth;
    int32 TextureHeight;
    TArray<uint8> TextureData;
    TAtomic<bool> bInitializedAndValid;

    FRenderCommandFence ReleaseFence;
};