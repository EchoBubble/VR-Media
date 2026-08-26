// Copyright 2019 UNAmedia. All rights reserved.

#include "PanoramicInteractionComponent.h"

#include "PanoramicBlueprintFunctionLibrary.h"

#include "PanoramicWidgetComponent.h"
#include "PanoramicWidgetViewfinderInterface.h"
#include "PanoramicDirector.h"

#include "UObject/ConstructorHelpers.h"

UPanoramicInteractionComponent::UPanoramicInteractionComponent(const FObjectInitializer& ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = true;

    static ConstructorHelpers::FClassFinder<UUserWidget> DefaultViewfinderBP(
        TEXT("UserWidget'/StereoPanoramicPlayer/Viewfinder/DefaultViewFinder.DefaultViewFinder_C'"));
    if (DefaultViewfinderBP.Succeeded())
    {
        ViewfinderClass = DefaultViewfinderBP.Class;
    }

    FocusDuration = 2.f;
    NavInFocusTime = 0.f;

    InteractionAutoMode = true;
    InteractionInputActionMode = false;

    ManualInteractionIsPending = false;

    CameraForwardAsTargetDirection = true;
    MouseCursorAsTargetDirection = false;
}

void UPanoramicInteractionComponent::SetupInputBindings(bool ToBind)
{
    UInputComponent* PlayerInputComponent = GetPlayerController()->InputComponent;
    check(PlayerInputComponent);

    if (ToBind)
    {
        check(!MoveActionBinding.IsSet());

        if (!InputActionName.IsNone())
        {
            FInputActionBinding& Binding = PlayerInputComponent->BindAction(InputActionName, IE_Pressed, this, &UPanoramicInteractionComponent::OnMoveActionFired);
            Binding.bConsumeInput = false;

            MoveActionBinding = Binding;
        }
    }
	else if (MoveActionBinding.IsSet())
	{
		PlayerInputComponent->RemoveActionBinding(MoveActionBinding.GetValue().GetHandle());
		MoveActionBinding.Reset();
	}
}

void UPanoramicInteractionComponent::CreateViewFinder()
{
    check(!Viewfinder);

    Viewfinder = NewObject<UPanoramicWidgetComponent>(this, TEXT("ViewFinderComponent"));
    Viewfinder->SetRelativeScale3D({ .1f, .1f, .1f });
    Viewfinder->TranslucencySortPriority = static_cast<int32>(EPanoramicTranslucencySortPriorities::Viewfinder);
    Viewfinder->SetBlendMode(EWidgetBlendMode::Transparent);
    Viewfinder->WorldPositionOffsetEnabled = true;
    Viewfinder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Viewfinder->SetWidgetClass(ViewfinderClass);

    USceneComponent* Root(GetDirector()->PanoramicSphere->GetDefaultAttachComponent());
    if (ensure(Root))
    {
        Viewfinder->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
    }

    Viewfinder->RegisterComponent();

	// Notify the Viewfinder UMG Widget if it implements our custom interface.
	UUserWidget * ViewfinderWidget = Viewfinder->GetUserWidgetObject();
	if (ViewfinderWidget != nullptr && ViewfinderWidget->GetClass()->ImplementsInterface(UPanoramicWidgetViewfinderInterface::StaticClass()))
	{
		IPanoramicWidgetViewfinderInterface::Execute_InitViewfinder(ViewfinderWidget, GetDirector());
	}
}

void UPanoramicInteractionComponent::DestroyViewFinder()
{
    if (Viewfinder)
    {
        Viewfinder->DestroyComponent();
        Viewfinder = nullptr;
    }
}

void UPanoramicInteractionComponent::OnMoveActionFired()
{
    ManualInteractionIsPending = true;
}

void UPanoramicInteractionComponent::InteractionHandling(float DeltaTime)
{
    FPanoramicStageNavigation Navigation;
    FVector HitPoint;
    if (TraceWorldDirections(Navigation, HitPoint))
    {
        if (ManualInteractionIsPending)
        {
            GetDirector()->MoveThrough(Navigation);
        }
        else if (InteractionAutoMode)
        {
            NavInFocusTime += DeltaTime;
            if (NavInFocusTime >= FocusDuration)
            {
                NavInFocusTime = 0.f;

                GetDirector()->MoveThrough(Navigation);
            }

            FVector Dir = HitPoint;
            Dir.Normalize();

            float Pitch = FMath::Asin(FVector::DotProduct(Dir, FVector::UpVector)) * 180.0f / PI;
            float Yaw = (FMath::Atan2(Dir.Y, Dir.X) - FMath::Atan2(0.f, 1.f)) * 180.0f / PI;
            // here -180.f is because widgets should be in front of us otherwise they will be culled
            FRotator Rot(-Pitch, Yaw - 180.0f, /*roll*/ 0.f);

            if (!Viewfinder)
                CreateViewFinder();

            Viewfinder->SetWorldLocation(HitPoint);
            Viewfinder->SetWorldRotation(Rot);
        }
    }
    else
    {
        NavInFocusTime = 0.f;

        DestroyViewFinder();
    }
}

class APanoramicDirector* UPanoramicInteractionComponent::GetDirector() const
{
    // the owner of this component should be the director
    // assert if this precodition is not true anymore
    return CastChecked<APanoramicDirector>(GetOwner());
}

class APlayerController* UPanoramicInteractionComponent::GetPlayerController() const
{
    //@TODO: find a more robust way to get the actual player controller
    return GetWorld()->GetFirstPlayerController();
}

FVector UPanoramicInteractionComponent::CameraWorldDirection() const
{
    FRotator Rot = GetPlayerController()->PlayerCameraManager->GetCameraRotation();
    return Rot.Vector();
}

FVector UPanoramicInteractionComponent::MouseWorldDirection() const
{
    FVector WorldDirection;
    FVector WorldLocation;

    GetPlayerController()->DeprojectMousePositionToWorld(WorldLocation, WorldDirection);

    return WorldDirection;
}

bool UPanoramicInteractionComponent::TraceWorldDirections(FPanoramicStageNavigation& OutNavigation, FVector& OutHitPoint)
{
    auto Stage(GetDirector()->GetCurrentStage());
    if (!Stage)
        return false;

    for (auto& Dir : WorldTargetDirections)
    {
        if (UPanoramicBlueprintFunctionLibrary::TracePanoramicStageNav(
            GetWorld(), Stage, GetDirector()->PanoramicSphere, Dir, OutNavigation))
        {
            // we don't need to perform a ray-sphere intersection
            // this approximation is enough
            OutHitPoint = Dir * 150.0f;

            return true;
        }
    }

    return false;
}

void UPanoramicInteractionComponent::PushCustomWorldTargetDirection(FVector WorldTargetDirection)
{
    CustomWorldTargetDirections.Add(WorldTargetDirection);
}

void UPanoramicInteractionComponent::TryToInteractManually()
{
    ManualInteractionIsPending = true;
}

void UPanoramicInteractionComponent::OnPanoramicModeEntered()
{
    SetupInputBindings(true);
}

void UPanoramicInteractionComponent::OnPanoramicModeExited()
{
    SetupInputBindings(false);

    DestroyViewFinder();
}

void UPanoramicInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    WorldTargetDirections = CustomWorldTargetDirections;

    if(CameraForwardAsTargetDirection)
        WorldTargetDirections.Add(CameraWorldDirection());

    if(MouseCursorAsTargetDirection)
        WorldTargetDirections.Add(MouseWorldDirection());

    InteractionHandling(DeltaTime);

    ManualInteractionIsPending = false;

    CustomWorldTargetDirections.Empty();
}
