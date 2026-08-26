// Copyright 2019 UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

#include "ProceduralSphereComponent.generated.h"

/**
 * Procedural Sphere Component
 */
UCLASS(hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent), ClassGroup = Rendering)
class UProceduralSphereComponent : public UProceduralMeshComponent
{
    GENERATED_UCLASS_BODY()

public:

    // Generate the sphere mesh
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer")
    void GenerateMesh(float Radius, int32 NumOfParallels, int32 NumOfMeridians);

private:

    //~ Begin USceneComponent Interface.
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    //~ End USceneComponent Interface.
};
