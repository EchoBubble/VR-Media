// Copyright 2019 UNAmedia. All rights reserved.
/** @file */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "PanoramicPosition.generated.h"

/**
 * The units to express a position in the panoramic scene
 */
UENUM(BlueprintType)
enum class EPanoramicUnits : uint8
{
    /** Coordinates are expressed in pixels ( X:[0,width), Y:[0,height) ) */
    Pixels  UMETA(DisplayName = "Pixels"),
    /** Coordinates are expressed in UV space ( X:[0,1], Y:[0,1] ) */
    UV      UMETA(DisplayName = "UV space"),
    /** Coordinates are expressed in degrees (yaw and pitch rotations) */
    Degrees UMETA(DisplayName = "Degrees (yaw and pitch rotations)")
};

/**
 * A position in the panoramic scene
 */
USTRUCT(BlueprintType)
struct FPanoramicPosition
{
    GENERATED_BODY()

    /** The units to express this position */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "StereoPanoramicPlayer")
    EPanoramicUnits Units = EPanoramicUnits::Pixels;

    /** The position in the specified units @see EPanoramicUnits */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "StereoPanoramicPlayer")
    FVector2D Value = FVector2D::ZeroVector;
};
