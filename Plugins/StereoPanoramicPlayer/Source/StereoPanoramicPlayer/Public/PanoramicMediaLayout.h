// Copyright 2019 UNAmedia. All rights reserved.
/** @file */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

/**
 * Options for the media layout.
 * It can be stereoscopic (over/under) or mono.
 * Stereoscopic (over/under) means that the source is divided in two logically.
 * In the top part there is the left-eye source, and in the bottom the right-eye source.
 */
UENUM(BlueprintType)
enum class EPanoramicMediaLayout : uint8
{
    /** Stereoscopic (over/under) media layout */
    StereoOverUnder  UMETA(DisplayName = "Stereoscopic - Over/under"),
    /** Mono media layout */
    Mono             UMETA(DisplayName = "Mono")
};
