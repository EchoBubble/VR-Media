// Copyright 2019 UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"

#include "PanoramicStage.h"

#include "PanoramicWidgetComponent.generated.h"

/**
 * UPanoramicWidgetComponent extends the UWidgetComponent 
 * to overwrite the default material, and to associate a stage navigation data
 * @see FPanoramicStageNavigation
 */
UCLASS(BlueprintType)
class STEREOPANORAMICPLAYER_API UPanoramicWidgetComponent : public UWidgetComponent
{
    GENERATED_UCLASS_BODY()

public:

    //~ Begin UPrimitiveComponent Interface
    /// Override UPrimitiveComponent::GetMaterial()
    virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
    //~ End UPrimitiveComponent Interface

    //~ Begin USceneComponent Interface.
    /// Override USceneComponent::CalcBounds()
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    //~ End USceneComponent Interface.

public:

    /** The stage navigation associated to this widget instance */
    TOptional<FPanoramicStageNavigation> Navigation;

    /** Indicates if the world position offset
        should be enabled or not in the material. */
    bool WorldPositionOffsetEnabled;

private:

    /** The material instance used to render the widget 
        when WorldPositionOffsetEnabled is true */
    UPROPERTY()
    UMaterialInstance* Material[2];

    /** The material instance used to render the widget
        when WorldPositionOffsetEnabled is false */
    UPROPERTY()
    UMaterialInstance* MaterialNoOffset[2];
};
