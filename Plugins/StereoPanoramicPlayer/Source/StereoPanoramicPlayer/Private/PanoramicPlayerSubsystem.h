// Copyright UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "Subsystems/WorldSubsystem.h"

#include "PanoramicPlayerSubsystem.generated.h"

class UTexture2D;
class UNavigationTexture;

/*
 * World subsystem that act as a manager
 */
UCLASS()
class UPanoramicPlayerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	UNavigationTexture* ResolveNavigationTexture(UTexture2D* InTexture);

	void ClearCachedNavigationTextures();

private:

	// map the panoramic stage with its navigation texture (it could be nullptr)
	UPROPERTY()
	TMap<const UTexture2D*, UNavigationTexture*> ResolvedTextures;
};
