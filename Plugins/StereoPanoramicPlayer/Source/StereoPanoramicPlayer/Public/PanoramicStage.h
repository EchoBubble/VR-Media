// Copyright 2019 UNAmedia. All rights reserved.
/** @file */

#pragma once

#include "CoreMinimal.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/DataAsset.h"

#include "MediaSource.h"
#include "Components/WidgetComponent.h"

#include "PanoramicMediaLayout.h"
#include "PanoramicPosition.h"

#include "PanoramicStage.generated.h"

/**
 * A stage navigation allows to switch from one stage to another one.
 */
USTRUCT(BlueprintType)
struct STEREOPANORAMICPLAYER_API FPanoramicStageNavigation
{
    GENERATED_BODY()

    FPanoramicStageNavigation();

    /** An optional UserWidget subclass to create and display an instance of */
    UPROPERTY(EditInstanceOnly, Category = UserInterface)
    TSubclassOf<UUserWidget> WidgetClass;

    /** Where the widget should be placed, editable/available only if WidgetClass is not null */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    FPanoramicPosition WidgetPosition;

    /** The widget scaling factor, editable/available only if WidgetClass is not null */
    UPROPERTY(EditAnywhere, Category = "StereoPanoramicPlayer")
    float WidgetScale;

    /** An optional color.
        This color is compared with the current focusing color of the stage's NavigationLookupTexture.
        When colors match, this stage navigation became active and the DestinationStage is used to move
        on the next stage. */
    UPROPERTY(EditInstanceOnly, meta = (HideAlphaChannel), Category = "StereoPanoramicPlayer")
    FColor LookupColor;

    /** The destination stage. Can be null, in this case the experience ends. */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    TSoftObjectPtr<class UPanoramicStage> DestinationStage;

    /** Flag to enable the YawRotation property (TOptional type can't be used in editor) */
    UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle), Category = "StereoPanoramicPlayer")
    bool YawRotationSetFlag;

    /** An additional yaw rotation (in degrees) to apply to the mesh sphere.
        This can be useful to align two stages together.
        If YawRotationSetFlag is true, YawRotation overwrites UPanoramicStage::YawRotation. */
    UPROPERTY(EditInstanceOnly, meta = (editcondition = "YawRotationSetFlag"), Category = "StereoPanoramicPlayer")
    float YawRotation;
};

/**
 * The action to perform when the media is over
 */
UENUM(BlueprintType)
enum class EOnMediaEndReachedDefaultBehaviour : uint8
{
    /** Do nothing */
    DoNothing             UMETA(DisplayName = "Do nothing"),
    /** Go back to the previous stage */
    GoBackToPreviousStage UMETA(DisplayName = "Go back to the previous stage"),
    /** Go to the choosen stage */
    GoToStage             UMETA(DisplayName = "Go to stage"),
    /** Exit from the experience */
    ExitFromExperience    UMETA(DisplayName = "Exit from experience")
};

/**
 * The media type.
 * Select the 'movie' if you want to play a movie source,
 * otherwise the 'image' if you want to show a static image
 */
UENUM(BlueprintType)
enum class EPanoramicMediaType : uint8
{
    /** Movie media type. */
    Movie  UMETA(DisplayName = "Movie"),
    /** Image type. */
    Image  UMETA(DisplayName = "Image")
};

/**
 * Collects data for a movie stage.
 */
USTRUCT(BlueprintType)
struct STEREOPANORAMICPLAYER_API FPanoramicStageMovieData
{
    GENERATED_BODY()

    /** The media source to play */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    UMediaSource* PanoramicMediaSource = nullptr;

    /** Indicates if the media should be played in loop or not */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    bool PlaybackLoop = false;

    /** The behaviour to have when the media reach the end */
    UPROPERTY(EditInstanceOnly, meta = (EditCondition = "!PlaybackLoop"), Category = "StereoPanoramicPlayer")
    EOnMediaEndReachedDefaultBehaviour OnMediaEndReachedDefaultBehaviour = EOnMediaEndReachedDefaultBehaviour::DoNothing;

    /** The destination stage when the media reach the end 
        and the OnMediaEndReachedDefaultBehaviour is GoToStage @see EOnMediaEndReachedDefaultBehaviour */
    UPROPERTY(EditInstanceOnly, meta = (EditCondition = "!PlaybackLoop"), Category = "StereoPanoramicPlayer")
    TSoftObjectPtr<class UPanoramicStage> DestinationStageOnMediaEndReached;
};

/**
 * Collects data for an image stage.
 */
USTRUCT(BlueprintType)
struct STEREOPANORAMICPLAYER_API FPanoramicStageImageData
{
    GENERATED_BODY()

    /** The texture to show */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    UTexture* PanoramicTexture = nullptr;
};

/**
 * Defines a stage of a panoramic experience.
 * What and how to be played and how to move from one
 * stage to the next one.
 */
UCLASS(BlueprintType)
class STEREOPANORAMICPLAYER_API UPanoramicStage : public UDataAsset
{
    GENERATED_UCLASS_BODY()

public:
    
    /** An optional name */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    FString Name;

    /** The media type. @see EPanoramicMediaType */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    EPanoramicMediaType MediaType;

    /** The media layout. @see EPanoramicMediaLayout */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    EPanoramicMediaLayout MediaLayout;

    /** Movie stage data. @see FPanoramicStageMovieData */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    FPanoramicStageMovieData MovieData;

    /** Image stage data. @see FPanoramicStageImageData */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    FPanoramicStageImageData ImageData;

    /** An optional texture used for the navigation.
    The texture must have a mono layout even if the MediaLayout is stereoscopic.
    The texture could have a different size compared to the Media but
        the aspect ratio should match.
    Be sure to disable sRGB as your masks should not be Gamma corrected.
    Set Compression settings to Userinterface2D (RGBA) and the Mip Gen Settings to NoMipmaps;
    "Never Stream" should be set to true.
    @see FPanoramicStageNavigation::LookupColor */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    UTexture2D* NavigationLookupTexture;

    /** An additional yaw rotation (in degrees) to apply to the mesh sphere.
        This can be useful to align two stages together. 
        It is ignored if FPanoramicStageNavigation::YawRotationSetFlag is true. */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    float YawRotation;

    /** Collects all stage navigations to switch stage. @see FPanoramicStageNavigation */
    UPROPERTY(EditInstanceOnly, Category = "StereoPanoramicPlayer")
    TArray<FPanoramicStageNavigation> Navigation;
};
