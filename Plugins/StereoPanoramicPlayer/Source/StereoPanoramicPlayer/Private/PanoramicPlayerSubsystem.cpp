// Copyright UNAmedia. All rights reserved.

#include "PanoramicPlayerSubsystem.h"

#include "NavigationTexture.h"

UNavigationTexture* UPanoramicPlayerSubsystem::ResolveNavigationTexture(UTexture2D* InTexture)
{
	check(InTexture);

	UNavigationTexture* const * TextureIt = ResolvedTextures.Find(InTexture);
	if (TextureIt != nullptr)
	{
		return *TextureIt;
	}

	// warning: it could be nullptr
	UNavigationTexture* Texture = UNavigationTexture::MakeFromTexture(this, InTexture);

	ResolvedTextures.Add(InTexture, Texture);

	return Texture;
}

void UPanoramicPlayerSubsystem::ClearCachedNavigationTextures()
{
	for (auto& Pair : ResolvedTextures)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->MarkAsGarbage();
		}
	}
	ResolvedTextures.Empty();
}

