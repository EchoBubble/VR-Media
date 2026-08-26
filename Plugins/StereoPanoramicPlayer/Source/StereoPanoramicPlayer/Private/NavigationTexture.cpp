// Copyright UNAmedia. All rights reserved.

#include "NavigationTexture.h"

#include "StereoPanoramicPlayer.h"

#include "Engine/Texture2D.h"
#include "Rendering/Texture2DResource.h"
#include "RenderUtils.h"

UNavigationTexture::UNavigationTexture()
    :
    TextureWidth(0),
    TextureHeight(0),
    bInitializedAndValid(false)
{

}

UNavigationTexture* UNavigationTexture::MakeFromTexture(UObject* Outer, UTexture2D* InTexture)
{
    if (!InTexture)
    {
        UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("Unable to create the NavigationTexture: a valid UTexture2D instance is needed."));
        return nullptr;
    }

    EPixelFormat PixelFormat = InTexture->GetPixelFormat();
    if (PixelFormat != EPixelFormat::PF_B8G8R8A8)
    {
        UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("Unable to create the NavigationTexture: the UTexture2D instance has an invalid pixel format. It is '%s' instead of 'PF_B8G8R8A8';"
            " You have to set the ***Compression Settings*** of your texture to ''UserInterface2D (RGBA)''."), GetPixelFormatString(PixelFormat));
        return nullptr;
    }

    UNavigationTexture* NavTexture = NewObject<UNavigationTexture>(Outer);
    if (!NavTexture->InitFromTexture(InTexture))
    {
        NavTexture->MarkAsGarbage();
        return nullptr;
    }

    return NavTexture;
}

bool UNavigationTexture::InitFromTexture(UTexture2D* InTexture)
{
    check(InTexture);
    check(InTexture->GetPixelFormat() == EPixelFormat::PF_B8G8R8A8);

    bInitializedAndValid = false;

    FTexture2DRHIRef Texture2DRHI = InTexture->GetResource()->TextureRHI->GetTexture2D();
    check(Texture2DRHI.IsValid());

    TextureWidth = Texture2DRHI->GetSizeX();
    TextureHeight = Texture2DRHI->GetSizeY();
    SrcTexture = InTexture;
    
    check(TextureWidth > 0);
    check(TextureHeight > 0);

    // reserve the space for the buffer
    static_assert(sizeof(FColor) == 4 * sizeof(uint8), "size mismatch");
    const int32 BufferSize = sizeof(FColor) * TextureWidth * TextureHeight;
    TextureData.SetNum(BufferSize);

    uint8* DestinationBuffer = TextureData.GetData();

    UNavigationTexture* This = this;
    ENQUEUE_RENDER_COMMAND(ReadNavigationTextureFromRHI)(
        [This, Texture2DRHI, DestinationBuffer, BufferSize](FRHICommandListImmediate& RHICmdList)
        {
            check(IsInRenderingThread());

            FReadSurfaceDataFlags ReadDataFlags;
            ReadDataFlags.SetLinearToGamma(false);

            FIntRect SourceRect(0, 0, Texture2DRHI->GetSizeX(), Texture2DRHI->GetSizeY());

            TArray<FColor> RawPixels;
            RawPixels.SetNum(SourceRect.Width() * SourceRect.Height());
            RHICmdList.ReadSurfaceData(Texture2DRHI, SourceRect, RawPixels, ReadDataFlags);

            check((RawPixels.Num() * sizeof(FColor)) == BufferSize);

            // copy the texture data from the RHI
            FMemory::Memcpy(DestinationBuffer, RawPixels.GetData(), BufferSize);

            This->bInitializedAndValid = true;
        });

    return true;
}

bool UNavigationTexture::Lookup(const FVector2D& UV, FColor& OutColor)
{
    if (!bInitializedAndValid)
    {
        UE_LOG(LogStereoPanoramicPlayer, Verbose, TEXT("Failed to lookup texture: the navigation texture is not initialized."));

        OutColor = FColor::Purple;
        return false;
    }

    const float u = UV.X;
    const float v = UV.Y;

    // clamp to [0,1]
    const int32 px = (int32)FMath::Clamp(u * TextureWidth, 0.f, TextureWidth - 1.f);
    const int32 py = (int32)FMath::Clamp(v * TextureHeight, 0.f, TextureHeight - 1.f);

    // get the pixel color
    const uint8* Buffer = TextureData.GetData();
    check(Buffer);

    static_assert(sizeof(FColor) == 4 * sizeof(uint8), "size mismatch");
    const uint8* PixelData = &Buffer[(int32)py * TextureWidth * 4 + (int32)px * 4];
    OutColor.B = PixelData[0];
    OutColor.G = PixelData[1];
    OutColor.R = PixelData[2];
    OutColor.A = PixelData[3];

    return true;
}

void UNavigationTexture::BeginDestroy()
{
    Super::BeginDestroy();

    // synchronize with the rendering thread by inserting a fence
    ReleaseFence.BeginFence();
}

bool UNavigationTexture::IsReadyForFinishDestroy()
{
    // ready to call FinishDestroy if the flushing fence has been hit
    return (Super::IsReadyForFinishDestroy() && ReleaseFence.IsFenceComplete());
}