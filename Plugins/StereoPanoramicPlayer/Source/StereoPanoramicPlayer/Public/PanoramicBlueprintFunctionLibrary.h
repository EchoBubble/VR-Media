// Copyright 2019 UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "PanoramicStage.h"

#include "PanoramicBlueprintFunctionLibrary.generated.h"

/**
 * Utilities functions accessible even from blueprints
 */
UCLASS()
class STEREOPANORAMICPLAYER_API UPanoramicBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    /** 
     *  Reads the pixel value of the given texture at the given UV coordinates.
     *  @param  WorldContextObject The World context object
     *  @param  NavLookupTex The navigation lookup texture
     *  @param  UV           The UV pixel coordinates where to read 
     * 	@param 	OutColor     The color in output
     *  @return TRUE if the texture is accessible, FALSE otherwise
     */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Utilities", meta = (WorldContext = WorldContextObject))
    static bool NavigationLookup(UObject* WorldContextObject, class UTexture2D* NavLookupTex, FVector2D UV, FColor& OutColor);

    /**
     *  Trace a ray against the world to find widget components.
     *  @param  WorldContextObject The World context object
     *  @param  InSphere           The panoramic sphere used for rendering
     *  @param  WorldDir           The dir (in world space) of the ray
     * 	@param 	OutWidget    	   The user widget component as output (if any)
     *  @return TRUE if a UWidgetComponent is found
     */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Utilities", meta = (WorldContext = WorldContextObject))
    static bool TracePanoramicWidget(UObject* WorldContextObject, const class APanoramicSphere* InSphere, 
                                FVector WorldDir, class UWidgetComponent*& OutWidget);

    /**
     *  Trace a ray against the world and return if a FPanoramicStageNavigation is found.
     *  @param  WorldContextObject The World context object
     *  @param  WorldDir           The dir (in world space) of the ray
     *  @param  InStage            The panoramic stage where you are
     *  @param  InSphere           The panoramic sphere used for rendering
     * 	@param 	OutStageNav        The stage navigation as output (if any)
     *  @return TRUE if a FPanoramicStageNavigation is found
     */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Utilities", meta = (WorldContext = WorldContextObject))
    static bool TracePanoramicStageNav(UObject* WorldContextObject, const UPanoramicStage* InStage, 
                                    const class APanoramicSphere* InSphere, FVector WorldDir, FPanoramicStageNavigation& OutStageNav);


    /**
     *  Constructs a panoramic widget
     *  @param  WorldContextObject The World context object
     *  @param  InWidgetClass the widget class type
     *  @param  OutWidget the new widget as output
     */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Utilities", meta = (WorldContext = WorldContextObject))
    static void NewPanoramicWidget(UObject* WorldContextObject, TSubclassOf<UUserWidget> InWidgetClass, class UWidgetComponent*& OutWidget);
};
