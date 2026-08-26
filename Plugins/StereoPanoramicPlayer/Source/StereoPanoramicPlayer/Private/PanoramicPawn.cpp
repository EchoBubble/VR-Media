// Copyright 2019 UNAmedia. All rights reserved.

#include "PanoramicPawn.h"

#include "Misc/ConfigCacheIni.h"
#include "GameFramework/PlayerInput.h"
#include "HeadMountedDisplayFunctionLibrary.h"

#include "Camera/CameraComponent.h"
#include "PanoramicWidgetComponent.h"

APanoramicPawn::APanoramicPawn(const FObjectInitializer& ObjectInitializer)
{
    CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
    CameraRoot->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
    CameraComponent->AttachToComponent(CameraRoot, FAttachmentTransformRules::KeepRelativeTransform);
    CameraComponent->bUsePawnControlRotation = true;

    CameraMovementFpsMode = true;
    CameraMovementDragMode = false;

    MouseDragging = false;

    DragSpeedScale = 0.25f;

    ShowMouseCursor = true;

    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(true);
}

void APanoramicPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindTouch(IE_Pressed, this, &APanoramicPawn::OnInputTouchPressed);
    PlayerInputComponent->BindTouch(IE_Released, this, &APanoramicPawn::OnInputTouchReleased);
    PlayerInputComponent->BindTouch(IE_Repeat, this, &APanoramicPawn::OnInputTouchMoved);

    bool bUseMouseForTouch(false);
    GConfig->GetBool(
        TEXT("/Script/Engine.InputSettings"),
        TEXT("bUseMouseForTouch"),
        bUseMouseForTouch,
        GInputIni
    );
    if (!bUseMouseForTouch)
    {
        constexpr char const * DefaultActionName = "PanoramicPawn_Drag";

        UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping(DefaultActionName, EKeys::LeftMouseButton));

        FInputActionBinding MousePressed(DefaultActionName, IE_Pressed);
        FInputActionBinding MouseReleased(DefaultActionName, IE_Released);

        MousePressed.ActionDelegate.GetDelegateForManualSet().BindLambda([this]()
        {
            FVector Location;
            GetPlayerController()->GetMousePosition(Location.X, Location.Y);
            MouseDragging = true;
            APanoramicPawn::OnInputTouchPressed(ETouchIndex::Touch1, Location);
        });
        MouseReleased.ActionDelegate.GetDelegateForManualSet().BindLambda([this]()
        {
            FVector Location;
            GetPlayerController()->GetMousePosition(Location.X, Location.Y);
            MouseDragging = false;
            APanoramicPawn::OnInputTouchReleased(ETouchIndex::Touch1, Location);
        });

        PlayerInputComponent->AddActionBinding(MousePressed);
        PlayerInputComponent->AddActionBinding(MouseReleased);
    }

    ApplyDragMode(CameraMovementDragMode);
}

void APanoramicPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (MouseDragging)
    {
        FVector Location;
        GetPlayerController()->GetMousePosition(Location.X, Location.Y);
        APanoramicPawn::OnInputTouchMoved(ETouchIndex::Touch1, Location);
    }
}

void APanoramicPawn::BeginPlay()
{
    Super::BeginPlay();

    // in VR we have to disable UsePawnControlRotation
    CameraComponent->bUsePawnControlRotation = !UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
}

void APanoramicPawn::AddControllerYawInput(float Val)
{
    if (CameraMovementFpsMode)
        Super::AddControllerYawInput(Val);
}

void APanoramicPawn::AddControllerPitchInput(float Val)
{
    if (CameraMovementFpsMode)
        Super::AddControllerPitchInput(Val);
}

void APanoramicPawn::ApplyDragMode(bool InDragMode)
{
    CameraMovementDragMode = InDragMode;

    if (CameraMovementDragMode)
    {
        GetPlayerController()->bShowMouseCursor = ShowMouseCursor;

        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
        InputMode.SetHideCursorDuringCapture(false);

        GetPlayerController()->SetInputMode(InputMode);
    }
    else
    {
        FInputModeGameOnly InputMode;
        GetPlayerController()->SetInputMode(InputMode);
    }
}

void APanoramicPawn::OnInputTouchPressed(ETouchIndex::Type FingerIndex, FVector Location)
{
    LastTouchLocation = Location;
}

void APanoramicPawn::OnInputTouchReleased(ETouchIndex::Type FingerIndex, FVector Location)
{
}

void APanoramicPawn::OnInputTouchMoved(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (!CameraMovementDragMode)
        return;

    FVector Movement = Location - LastTouchLocation;
    LastTouchLocation = Location;

    float Yaw = Movement.X * DragSpeedScale;
    float Pitch = -Movement.Y * DragSpeedScale;

    Super::AddControllerYawInput(Yaw);
    Super::AddControllerPitchInput(Pitch);
}

class APlayerController* APanoramicPawn::GetPlayerController() const
{
    //@TODO: find a more robust way to get the actual player controller
    return GetWorld()->GetFirstPlayerController();
}
