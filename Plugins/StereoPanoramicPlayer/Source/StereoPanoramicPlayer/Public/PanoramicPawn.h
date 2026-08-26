// Copyright 2019 UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "PanoramicPawn.generated.h"

/**
 * The default pawn for the panoramic experience.
 * Out of the box camera movement behaviours (fps style, mouse dragging / swipe gesture on mobile)
 */
UCLASS()
class STEREOPANORAMICPLAYER_API APanoramicPawn : public ADefaultPawn
{
    GENERATED_UCLASS_BODY()

public:

    /** In this mode the camera moves like on an fps game. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Camera Movement")
    bool CameraMovementFpsMode;

    /** In this mode to move the camera the user have to click, hold pressed and drag (swipe on mobile). */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = ApplyDragMode, Category = "StereoPanoramicPlayer|Camera Movement")
    bool CameraMovementDragMode;

    /** The scale speed factor during dragging/swiping. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Camera Movement", meta = (editcondition = "CameraMovementDragMode"))
    float DragSpeedScale;

    /** Show mouse cursor */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Camera Movement", meta = (editcondition = "CameraMovementDragMode"))
    bool ShowMouseCursor;

    /** The camera component attached to the pawn */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer|Camera Movement")
    class UCameraComponent* CameraComponent;

    /** Parent component of CameraComponent */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer|Camera Movement")
    class USceneComponent* CameraRoot;

protected:

    /** Override ADefaultPawn::SetupPlayerInputComponent */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    /** Override ADefaultPawn::AddControllerYawInput */
    virtual void AddControllerYawInput(float Val) override;

    /** Override ADefaultPawn::AddControllerPitchInput */
    virtual void AddControllerPitchInput(float Val) override;

    /** Override AActor::Tick */
    virtual void Tick(float DeltaSeconds) override;

    /** Override AActor::BeginPlay */
    virtual void BeginPlay() override;

private:

    UFUNCTION(BlueprintSetter)
    void ApplyDragMode(bool InDragMode);

    void OnInputTouchPressed(ETouchIndex::Type FingerIndex, FVector Location);
    void OnInputTouchReleased(ETouchIndex::Type FingerIndex, FVector Location);
    void OnInputTouchMoved(ETouchIndex::Type FingerIndex, FVector Location);

    class APlayerController* GetPlayerController() const;

private:

    FVector LastTouchLocation;
    bool MouseDragging;
};
