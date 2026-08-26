// Copyright 2019 UNAmedia. All rights reserved.

#include "PanoramicWidgetComponent.h"

#include "PanoramicTranslucencySortPriorities.h"

#define MATERIAL_INSTANCE_INITIALIZER(VarName, AssetRef) \
    static ConstructorHelpers::FObjectFinder<UMaterialInstance> AssetRef##Asset( \
        TEXT("/StereoPanoramicPlayer/Materials/Widgets/" #AssetRef "." #AssetRef)); \
    if (AssetRef ## Asset.Succeeded()) \
    { \
        VarName = AssetRef##Asset.Object; \
    }

UPanoramicWidgetComponent::UPanoramicWidgetComponent(const FObjectInitializer& ObjectInitializer)
{
    // initialize translucency materials
    MATERIAL_INSTANCE_INITIALIZER(Material[0], WidgetMaterial)
    MATERIAL_INSTANCE_INITIALIZER(MaterialNoOffset[0], WidgetMaterial_NoOffset)

    // initialize opaque materials
    MATERIAL_INSTANCE_INITIALIZER(Material[1], WidgetOpaqueMaterial)
    MATERIAL_INSTANCE_INITIALIZER(MaterialNoOffset[1], WidgetOpaqueMaterial_NoOffset)

    TranslucencySortPriority = static_cast<int32>(EPanoramicTranslucencySortPriorities::Widget);
    SetBlendMode(EWidgetBlendMode::Transparent);

    WorldPositionOffsetEnabled = true;
    bDrawAtDesiredSize = true;
}

#undef MATERIAL_INSTANCE_INITIALIZER

UMaterialInterface* UPanoramicWidgetComponent::GetMaterial(int32 MaterialIndex) const
{
    const int Index = BlendMode == EWidgetBlendMode::Transparent ? 0 : 1;

    if (WorldPositionOffsetEnabled && Material[Index])
        return Material[Index];
    else if (!WorldPositionOffsetEnabled && MaterialNoOffset[Index])
        return MaterialNoOffset[Index];
    else
        return Super::GetMaterial(MaterialIndex);
}

FBoxSphereBounds UPanoramicWidgetComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    // Returns huge bounds to avoid culling
    return FBoxSphereBounds(FVector::ZeroVector, FVector(WORLD_MAX), WORLD_MAX); // .TransformBy(LocalToWorld);
}
