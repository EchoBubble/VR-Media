// Copyright 2019 UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DataAsset.h"

#include "PanoramicStage.h"

#include "PanoramicExperience.generated.h"

/**
 * Collects data of a panoramic experience.
 */
UCLASS(BlueprintType)
class STEREOPANORAMICPLAYER_API UPanoramicExperience : public UDataAsset
{
    GENERATED_BODY()

    UPanoramicExperience();

public:

    /** The entry stage */
    UPROPERTY(EditAnywhere, Category = "StereoPanoramicPlayer")
    UPanoramicStage* EntryStage;

    /** Indicates if the pawn rotation at the experience start should be restored on exit. */
    UPROPERTY(EditAnywhere, Category = "StereoPanoramicPlayer")
    bool RestoreInitialPawnRotationOnExit;
};
