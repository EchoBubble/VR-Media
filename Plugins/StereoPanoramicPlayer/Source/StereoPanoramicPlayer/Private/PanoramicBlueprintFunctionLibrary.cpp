// Copyright 2019 UNAmedia. All rights reserved.

#include "PanoramicBlueprintFunctionLibrary.h"

#include "PanoramicWidgetComponent.h"
#include "PanoramicSphere.h"
#include "PanoramicPlayerSubsystem.h"

#include "NavigationTexture.h"
#include "Engine/Engine.h"
#include "Runtime/Launch/Resources/Version.h"

// Define that controls if enable the new code to access the texture data due to an engine bug in UE 5.0 and UE 5.1 (@see Bug #1023)
#define SPP_USE_NEW_NAVIGATION_LOOKUP_TEXTURE_DATA_ACCESS (ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 0 || ENGINE_MINOR_VERSION == 1))

bool UPanoramicBlueprintFunctionLibrary::NavigationLookup(UObject* WorldContextObject, UTexture2D* NavLookupTex, FVector2D UV, FColor& OutColor)
{
#if SPP_USE_NEW_NAVIGATION_LOOKUP_TEXTURE_DATA_ACCESS
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return false;
    }

    UPanoramicPlayerSubsystem* PPSubsystem = World->GetSubsystem<UPanoramicPlayerSubsystem>();
    UNavigationTexture* NavigationTexture = PPSubsystem->ResolveNavigationTexture(NavLookupTex);

    if (!NavigationTexture)
    {
        return false;
    }

    return NavigationTexture->Lookup(UV, OutColor);
#else
    if (!NavLookupTex)
        return false;

    FTexture2DMipMap& Mip = NavLookupTex->GetPlatformData()->Mips[0];
    FByteBulkData& RawImageData = Mip.BulkData;

    int32 Width = Mip.SizeX;
    int32 Height = Mip.SizeY;

    float u = UV.X;
    float v = UV.Y;

    // clamp to [0,1]
    float px = FMath::Clamp(u * Width, 0.f, Width - 1.f);
    float py = FMath::Clamp(v * Height, 0.f, Height - 1.f);

    // get the pixel color
    const uint8* ImageData = static_cast<const uint8*>(RawImageData.Lock(LOCK_READ_ONLY));
    if (ImageData)
    {
        static_assert(sizeof(FColor) == 4 * sizeof(uint8), "size mismatch");
        const uint8* PixelData = &ImageData[(int32)py * Width * 4 + (int32)px * 4];
        OutColor.B = PixelData[0];
        OutColor.G = PixelData[1];
        OutColor.R = PixelData[2];
        OutColor.A = PixelData[3];
    }
    RawImageData.Unlock();

    return ImageData != nullptr;
#endif
}

bool UPanoramicBlueprintFunctionLibrary::TracePanoramicWidget(UObject * WorldContextObject, 
    const class APanoramicSphere* InSphere, FVector WorldDir, class UWidgetComponent*& OutWidget)
{
    check(WorldContextObject);

	OutWidget = nullptr;

    if (!InSphere)
        return false;

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
        return false;

    FCollisionQueryParams TraceParams = FCollisionQueryParams(FName(TEXT("PP_Trace")), true);
    TraceParams.bTraceComplex = false;
    TraceParams.bReturnPhysicalMaterial = false;

    FVector RayFrom = InSphere->GetActorLocation();

    // this bias is needed cos the sphere-widget distance is bigger on widget edges than on its center!
    // DistanceFromWidgets returns the sphere-widget distance regarding their centers.
    // we could calculate it precisely but we should know the max widget-size...
    const float Bias = 1.2f;
    FVector RayTo = RayFrom + WorldDir * (InSphere->DistanceFromWidgets() * Bias);

    TArray<FHitResult> OutHits;

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

    FHitResult OutHit;
    World->LineTraceMultiByObjectType(
        OutHits,
        RayFrom,
        RayTo,
        Objects,
        TraceParams);

    for (auto& Comp : OutHits)
    {
        auto Widget = Cast<UPanoramicWidgetComponent>(Comp.Component);
        if (Widget)
        {
            OutWidget = Widget;
            return true;
        }
    }

    return false;
}

bool UPanoramicBlueprintFunctionLibrary::TracePanoramicStageNav(UObject * WorldContextObject, const UPanoramicStage* InStage,
    const APanoramicSphere* Sphere, FVector WorldDir, FPanoramicStageNavigation& OutStageNav)
{
    check(WorldContextObject);

    if (!InStage || !Sphere)
        return false;

    FColor OutColor;
    UWidgetComponent* OutComponent;

    FVector2D UV = Sphere->UVMap(WorldDir);

    if (TracePanoramicWidget(WorldContextObject, Sphere, WorldDir, OutComponent))
    {
        auto WidgetComponent = Cast<UPanoramicWidgetComponent>(OutComponent);
        if (WidgetComponent && WidgetComponent->Navigation.IsSet())
        {
            OutStageNav = WidgetComponent->Navigation.GetValue();
            return true;
        }
    }

    if (InStage->NavigationLookupTexture)
    {
        if (NavigationLookup(WorldContextObject, InStage->NavigationLookupTexture, UV, OutColor))
        {
            for (auto& Nav : InStage->Navigation)
            {
                if (Nav.LookupColor.WithAlpha(0) == OutColor.WithAlpha(0)) // alpha channel is ignored
                {
                    OutStageNav = Nav;
                    return true;
                }
            }
        }
    }

    return false;
}

void UPanoramicBlueprintFunctionLibrary::NewPanoramicWidget(
    UObject* WorldContextObject, 
    TSubclassOf<UUserWidget> InWidgetClass,
    class UWidgetComponent*& OutWidget)
{
    static int Counter = 0;

    OutWidget = NewObject<UPanoramicWidgetComponent>(WorldContextObject, FName(*("Widget_" + FString::FromInt(Counter++))));
    OutWidget->SetWidgetClass(InWidgetClass);
}
