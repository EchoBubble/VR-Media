// Copyright 2019 UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/InputComponent.h"

#include "PanoramicInteractionComponent.generated.h"


/** The component used to perfom interactions with the stage navigations */
/// @cond
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
/// @endcond
class STEREOPANORAMICPLAYER_API UPanoramicInteractionComponent : public UActorComponent
{
    GENERATED_UCLASS_BODY()

public:

    /** Flag to enable the auto interaction mode. If true, when the user focus a navigation target,
        we move on the next stage automatically (@see FocusDuration) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Interaction")
    bool InteractionAutoMode;

    /** How long (in seconds) the user should focus a target to enter on the next stage.
        Put to zero to disable this mechanism. It works if the auto interaction mode is on (@see InteractionAutoMode) */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Interaction", meta = (editcondition = "InteractionAutoMode"))
    float FocusDuration;

    /** An optional UMG widget used to customize the viewfinder.
        Visible when the user is focusing a stage navigation and the auto interaction mode is on (@see InteractionAutoMode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Interaction", meta = (editcondition = "InteractionAutoMode"))
    TSubclassOf<class UUserWidget> ViewfinderClass;

    /** Flag to enable the input-action interaction mode. If true, we move on the target stage (if any) when the choosen
        input action is triggered (@see InputActionName) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Interaction")
    bool InteractionInputActionMode;

    /** The input action name used to trigger an interaction. (@see InteractionInputActionMode) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Interaction", meta = (editcondition = "InteractionInputActionMode"))
    FName InputActionName;

    /** Use the camera forward as target direction. Target Directions are used to interact with stage navigations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Interaction")
    bool CameraForwardAsTargetDirection;

    /** Use the mouse cursor as target direction. Target Directions are used to interact with stage navigations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Interaction")
    bool MouseCursorAsTargetDirection;

public:

    /** Add a custom target direction (should be normalized and in world space).
        It will be used to interact with stage navigations.
        The direction will be used automatically but you can call TryToInteractManually to try to interact 
        (to perform a transition if it's possible) manually and instantanially.
        Collected custom target directions are cleared at each tick. */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Interaction")
    void PushCustomWorldTargetDirection(FVector WorldTargetDirection);

    /** Try to interact with stage navigations manually and instantanially */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer|Interaction")
    void TryToInteractManually();

    /** Called by the director. Don't call it manually. */
    void OnPanoramicModeEntered();

    /** Called by the director. Don't call it manually. */
    void OnPanoramicModeExited();

    /** Override UActorComponent::TickComponent */
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

    void InteractionHandling(float DeltaTime);
    void OnMoveActionFired();
    void SetupInputBindings(bool Bind);

    void CreateViewFinder();
    void DestroyViewFinder();

    class APanoramicDirector* GetDirector() const;
    class APlayerController* GetPlayerController() const;

    FVector CameraWorldDirection() const;
    FVector MouseWorldDirection() const;

    bool TraceWorldDirections(struct FPanoramicStageNavigation& OutNavigation, FVector& OutHitPoint);

private:

    UPROPERTY()
    class UPanoramicWidgetComponent* Viewfinder;

    float NavInFocusTime;

    bool ManualInteractionIsPending;

    TOptional<FInputActionBinding> MoveActionBinding;

    // all target directions to process on this frame
    TArray<FVector> WorldTargetDirections;
    // custom target directions to be processed
    TArray<FVector> CustomWorldTargetDirections;
};
