// Copyright 2019 UNAmedia. All rights reserved.

#include "PanoramicDirector.h"

#include "MediaPlayer.h"
#include "Mediatexture.h"
#include "Camera/CameraComponent.h"

#include "MediaSoundComponent.h"

#include "StereoPanoramicPlayer.h"
#include "PanoramicPlayerSubsystem.h"
#include "PanoramicInteractionComponent.h"
#include "PanoramicBlueprintFunctionLibrary.h"
#include "PanoramicWidgetComponent.h"
#include "PanoramicStage.h"
#include "PanoramicExperience.h"
#include "PanoramicTranslucencySortPriorities.h"


APanoramicDirector::APanoramicDirector(const FObjectInitializer& ObjectInitializer)
{
    //PanoramicSphere = CreateDefaultSubobject<APanoramicSphere>(TEXT("PanoramicSphere"));
    PanoramicSphere = nullptr;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));

    MediaSoundComponent = CreateDefaultSubobject<UMediaSoundComponent>(TEXT("MediaSoundComponent"));

    InteractionComponent = CreateDefaultSubobject<UPanoramicInteractionComponent>(TEXT("InteractionComponent"));

    MediaPlayer = nullptr;
    CutOffCurve = nullptr;
    CutOffTexture = nullptr;
    CutOffAlphaCurve = nullptr;

    CutOffOnEnterAndExit = false;
    CutOffOnTransition = false;
    FadeOnEnterAndExit = false;
    FadeOnTransition = false;

    CutOffDuration = 1.f;
    FadeDuration = 1.f;
    TransitionTime = 0.f;

    FadeStatus = EFadeStatus::NO_FADING;

    BlendMode = ESphereBlendMode::Translucent;

    FrameYawRotation = 0.f;

    CurrentStage = nullptr;
    PrevStage = NextStage = nullptr;

    PrimaryActorTick.bCanEverTick = true;
}

void APanoramicDirector::BeginPlay()
{
    Super::BeginPlay();

    if (MediaPlayer)
    {
        MediaPlayer->OnEndReached.AddDynamic(this, &ThisClass::OnMediaEndReached);

        MediaSoundComponent->SetMediaPlayer(MediaPlayer);
    }

    // spawn into the world an instance of APanoramicSphere if it's not already set
    if (!PanoramicSphere)
    {
        FVector Location(FVector::ZeroVector);
        FRotator Rotation(FRotator::ZeroRotator);
        FActorSpawnParameters SpawnInfo;
        PanoramicSphere = GetWorld()->SpawnActor<APanoramicSphere>(Location, Rotation, SpawnInfo);

        PanoramicSphere->BlendMode = BlendMode;
    }

    if (Experience)
    {
        Play(Experience);
    }
}

bool APanoramicDirector::EnterOrExitTransition() const
{
    if (FadeStatus == EFadeStatus::NO_FADING)
        return false;

    return (CurrentStage == nullptr || NextStage.IsNull());
}

void APanoramicDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void APanoramicDirector::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateFading(DeltaTime);
}

void APanoramicDirector::Play(class UPanoramicExperience* InExperience, float InStartingYawRotation)
{
    if (IsPlaying())
        return;

    if (!InExperience)
        return;

    Experience = InExperience;

    FrameYawRotation = InStartingYawRotation;

    if (ScheduleToMove(Experience->EntryStage))
    {
        EnterInSpectatorMode();
    }
}

void APanoramicDirector::End()
{
    DoExit();
}

bool APanoramicDirector::IsPlaying() const
{
    return CurrentStage != nullptr || NextStage != nullptr;
}

void APanoramicDirector::FadeIn()
{
    FadeStatus = EFadeStatus::FADING_IN;
    TransitionTime = 0.f;
}

void APanoramicDirector::FadeOut()
{
    FadeStatus = EFadeStatus::FADING_OUT;
    TransitionTime = GetTransitionDuration() / 2.f;

    // if we are entering to the experience
    // for the fist time, the fade out is useless.
    // just skip it and move on the fade in.
    if (CurrentStage == nullptr)
    {
        TransitionTime = 0.f;
    }
}

float APanoramicDirector::GetFadeValue() const
{
    bool EnterOrExit = EnterOrExitTransition();
    if ((EnterOrExit && FadeOnEnterAndExit) || (!EnterOrExit && FadeOnTransition))
    {
        if (FadeDuration > 0 
            && FadeStatus != EFadeStatus::NO_FADING)
        {
            float time = FMath::Clamp((TransitionTime * 2.f) / FadeDuration, 0.f, 1.f);

            if (FadeCurve)
                return FMath::Clamp(FadeCurve->GetFloatValue(time), 0.f, 1.f);
        }
    }

    return 1.f;
}

float APanoramicDirector::GetCutOffValue() const
{
    bool EnterOrExit = EnterOrExitTransition();
    if ((EnterOrExit && CutOffOnEnterAndExit) || (!EnterOrExit && CutOffOnTransition))
    {
        if (CutOffDuration > 0
            && FadeStatus != EFadeStatus::NO_FADING)
        {
            float time = FMath::Clamp((TransitionTime * 2.f) / CutOffDuration, 0.f, 1.f);

            if (CutOffCurve)
                return FMath::Clamp(CutOffCurve->GetFloatValue(time), 0.f, 1.f);
        }
    }
    
    return 1.f;
}

float APanoramicDirector::GetCutOffAlphaValue() const
{
    // CutOffAlpha is take in account only if we are entering or exiting
    // into/from the panoramic experience.
    //if(!EnterOrExitTransition())
    //    return 1.0f;

    bool EnterOrExit = EnterOrExitTransition();
    if ((EnterOrExit && CutOffOnEnterAndExit) || (!EnterOrExit && CutOffOnTransition))
    {
        if (CutOffDuration > 0
            && FadeStatus != EFadeStatus::NO_FADING)
        {
            float time = FMath::Clamp((TransitionTime * 2.f) / CutOffDuration, 0.f, 1.f);

            if (CutOffAlphaCurve)
                return FMath::Clamp(CutOffAlphaCurve->GetFloatValue(time), 0.f, 1.f);
        }
    }

    return 1.f;
}

void APanoramicDirector::UpdateFading(float DeltaSeconds)
{
    float EndTime = GetTransitionDuration() / 2.f;

    switch (FadeStatus)
    {
        case EFadeStatus::FADING_IN:
        {
            TransitionTime = FMath::Clamp(TransitionTime + DeltaSeconds, 0.f, EndTime);
            if (TransitionTime >= EndTime)
                OnFadeInEnd();
            break;
        }
        case EFadeStatus::FADING_OUT:
        {
            TransitionTime = FMath::Clamp(TransitionTime - DeltaSeconds, 0.f, EndTime);
            if (TransitionTime <= 0.f)
                OnFadeOutEnd();
            break;
        }
    }

    if (PanoramicSphere)
    {
        PanoramicSphere->SetCutOffAlpha(GetCutOffAlphaValue());
        PanoramicSphere->SetCutOffIntensity(GetCutOffValue());
        PanoramicSphere->SetFadeIntensity(GetFadeValue());
    }
}

bool APanoramicDirector::HasNextStageLoadCompleted() const
{
    return LoadingStageHandle == nullptr && NextStage.IsValid();
}

float APanoramicDirector::GetTransitionDuration() const
{
    const bool EnterOrExit = EnterOrExitTransition();

    bool FadeOn = (EnterOrExit && FadeOnEnterAndExit) || (!EnterOrExit && FadeOnTransition);
    bool CutOffOn = (EnterOrExit && CutOffOnEnterAndExit) || (!EnterOrExit && CutOffOnTransition);

    return FMath::Max(FadeDuration * (FadeOn ? 1.0f : 0.0f),
                      CutOffDuration * (CutOffOn ? 1.0f : 0.0f));
}

void APanoramicDirector::OnFadeInEnd()
{
    CreateStageWidgets();

    FadeStatus = EFadeStatus::NO_FADING;
    PrevStage = CurrentStage;
    CurrentStage = NextStage.Get();
    NextStage = nullptr;
}

void APanoramicDirector::OnFadeOutEnd()
{
    //OnFadeOutEnd can be called more times...why?
    //- the stage is still loading;
    //- the PanoramicTexture is loaded but the texture streaming is still running

    if (HasNextStageLoadCompleted())
    {
        if (NextStage->MediaType == EPanoramicMediaType::Image)
        {
            UTexture2D* Texture = Cast<UTexture2D>(NextStage->ImageData.PanoramicTexture);
            if (Texture)
            {
                ForceTextureStreamInIfNeeded(Texture);

                if (!IsTextureReadyForRendering(Texture))
                    return; // Texture is still streaming in, retry next tick...
            }
        }
        else if (NextStage->MediaType == EPanoramicMediaType::Movie)
        {
            if (MediaPlayer)
            {
                if (MediaPlayer->GetUrl() != NextStage->MovieData.PanoramicMediaSource->GetUrl())
                {
                    MediaPlayer->OpenSource(NextStage->MovieData.PanoramicMediaSource);
                    return;
                }
                else if (MediaPlayer->IsPlaying())
                {
                    MediaPlayer->Pause();
                    MediaPlayer->Rewind();
                    return;
                }
                else if (MediaPlayer->IsPreparing())
                {
                    return; // mediaplayer is preparing to play the video, wait it before moving to the next stage
                }
            }
        }

        FadeStatus = EFadeStatus::NO_FADING;

        if (DoMove())
        {
            FadeIn();
        }
    }
    else if (NextStage.IsNull())
    {
        FadeStatus = EFadeStatus::NO_FADING;

        DoExit();
    }
}

void APanoramicDirector::OnMediaEndReached()
{
    if (!CurrentStage || !CanMove())
    {
        // Could happen that the media reach the end with a transition still running.
        // Handle this corner case warning the user and returning
        UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("The media has reached the end with a transition still running."
            " OnMediaEndReached will be ignored."));
        return;
    }
    
    check(CurrentStage->MediaType == EPanoramicMediaType::Movie);

    auto& Data = CurrentStage->MovieData;

    if (!Data.PlaybackLoop
        && Data.OnMediaEndReachedDefaultBehaviour != EOnMediaEndReachedDefaultBehaviour::DoNothing)
    {
        // the media is over, evaluate where to go...

        TSoftObjectPtr<class UPanoramicStage> Stage;
        switch (Data.OnMediaEndReachedDefaultBehaviour)
        {
        case EOnMediaEndReachedDefaultBehaviour::GoBackToPreviousStage:
            Stage = PrevStage;
            break;
        case EOnMediaEndReachedDefaultBehaviour::GoToStage:
            Stage = Data.DestinationStageOnMediaEndReached;
            break;
        case EOnMediaEndReachedDefaultBehaviour::ExitFromExperience:
            Stage = nullptr;
            break;
        default:
            check(false);
        }

        MoveOrExit(Stage);
    }
}

bool APanoramicDirector::CanMove() const
{
    return ((NextStage.IsNull())
        && (FadeStatus == EFadeStatus::NO_FADING));
}

void APanoramicDirector::MoveOrExit(TSoftObjectPtr<class UPanoramicStage> InNextStage)
{
    if (!CanMove())
        return;

    // Widgets are removed as soon as possible because
    // they are not influenced by the fading effect...
    ClearStageWidgets();

    if (!ScheduleToMove(InNextStage))
    {
        if (Experience && Experience->RestoreInitialPawnRotationOnExit)
        {
            FRotator ActualPawnRotation = GetWorld()->GetFirstPlayerController()->GetControlRotation();
            FRotator DiffRotation = InitialPawnRotation - ActualPawnRotation;

            // rotate the sphere (ignoring the pitch/roll)
            PanoramicSphere->SetActorRotation(PanoramicSphere->GetActorRotation() + FRotator(0.f, DiffRotation.Yaw, 0.f));

            // preserve the current pitch
            InitialPawnRotation.Pitch = ActualPawnRotation.Pitch;

            // rotate the pawn
            GetWorld()->GetFirstPlayerController()->SetControlRotation(InitialPawnRotation);
        }

        UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("Invalid stage name; exit from the panoramic experience."));

        // InNextStage is invalid, fade out to exit
        FadeOut();
    }
}

void APanoramicDirector::MoveThrough(const FPanoramicStageNavigation& Navigation)
{
    if (!CanMove())
        return;

    if (Navigation.YawRotationSetFlag)
        YawRotationFromNavigation = Navigation.YawRotation;

    MoveOrExit(Navigation.DestinationStage);
}

UPanoramicStage* APanoramicDirector::GetCurrentStage() const
{
    return CurrentStage;
}

bool APanoramicDirector::ScheduleToMove(TSoftObjectPtr<class UPanoramicStage> InNextStage)
{
    check(CanMove());

    if (InNextStage.IsNull())
        return false;

    NextStage = InNextStage;

    // Note: the callback "OnStageLoaded" could be called in a sync way
    // if the resource is already loaded!
    LoadingStageHandle = AssetLoader.RequestAsyncLoad(InNextStage.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &APanoramicDirector::OnStageLoaded));

    FadeOut();

    return true;
}

void APanoramicDirector::OnStageLoaded()
{
    check(IsInGameThread());

    // Could happen that the user has called End() to exit from the experience;
    // in this case, skip other checks...
    if (IsPlaying())
    {
        check(NextStage.IsValid());
        check(NextStage.Get() == LoadingStageHandle->GetLoadedAsset());
    }

    LoadingStageHandle = nullptr;
}

void APanoramicDirector::ForceTextureStreamInIfNeeded(UTexture2D* InTexture)
{
    if (!InTexture)
        return;

    if (InTexture->NeverStream)
        return;

    if (InTexture->bHasStreamingUpdatePending)
        return;

    InTexture->SetForceMipLevelsToBeResident(3600.0f);
}

bool APanoramicDirector::IsTextureReadyForRendering(UTexture2D* InTexture)
{
    if (!InTexture)
        return false;

    if (InTexture->NeverStream)
        return true;

    return (InTexture->GetNumMips() == InTexture->GetNumResidentMips());
}

bool APanoramicDirector::DoMove()
{
    check(NextStage);

    if (!IsValidStage(*NextStage))
    {
        DoExit();
        return false;
    }

    const float Yaw = FrameYawRotation +
                        (YawRotationFromNavigation.IsSet()
                        // rotations are negated here cos we are rotating the sphere
                        // but for the user make more sense to think about the viewer
                        // so a positive rotation is clockwise.
                        ? -YawRotationFromNavigation.GetValue()
                        : -NextStage->YawRotation);

    YawRotationFromNavigation.Reset();

    PanoramicSphere->SetActorRotation(FRotator(0.f, Yaw, 0.f));

    if (MediaPlayer && NextStage->MediaType == EPanoramicMediaType::Movie)
    {
        auto& Data = NextStage->MovieData;

        MediaPlayer->SetLooping(Data.PlaybackLoop);
        // the video source is opened at the end of the fade out so we can wait for the MediaPlayer when is ready to play
        // otherwise we could see black frames or frames of the previous played video!
        //MediaPlayer->OpenSource(Data.PanoramicMediaSource);
        MediaPlayer->Play();

        if (MediaTexture)
        {
            PanoramicSphere->Play(MediaTexture, NextStage->MediaLayout);
        }
        else
        {
            UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("The PanoramicDirection doesn't have a valid MediaTexture" 
                " but it's needed to play the source '%s'"), *Data.PanoramicMediaSource->GetPathName());
        }
    }
    else
    {
        if (MediaPlayer)
            MediaPlayer->Pause();

        PanoramicSphere->Play(NextStage->ImageData.PanoramicTexture, NextStage->MediaLayout);
    }

    if (PanoramicSphere)
    {
        if(CutOffTexture)
            PanoramicSphere->SetCutOffTexture(CutOffTexture);
        if(BackgroundTexture)
            PanoramicSphere->SetBackgroundTexture(BackgroundTexture);
    }

    OnStageChanged.Broadcast(NextStage.Get());

    return true;
}

void APanoramicDirector::DoExit()
{
    ClearStageWidgets();

    if (MediaPlayer)
        MediaPlayer->Pause();

    PanoramicSphere->End();
    ExitFromSpectatorMode();

    GetWorld()->GetSubsystem<UPanoramicPlayerSubsystem>()->ClearCachedNavigationTextures();

    CurrentStage = nullptr;
    PrevStage = NextStage = nullptr;
    FadeStatus = EFadeStatus::NO_FADING;
}

void APanoramicDirector::EnterInSpectatorMode()
{
    InitialPawnRotation = GetWorld()->GetFirstPlayerController()->GetControlRotation();

    InteractionComponent->OnPanoramicModeEntered();

    OnExperienceStarted.Broadcast();
}

void APanoramicDirector::ExitFromSpectatorMode()
{
    InteractionComponent->OnPanoramicModeExited();

    OnExperienceTerminated.Broadcast();
}

void APanoramicDirector::CreateStageWidgets()
{
    check(NextStage);

    ClearStageWidgets();

    for (auto& Nav : NextStage->Navigation)
    {
        if (!Nav.WidgetClass)
            continue;

        auto Widget = NewObject<UPanoramicWidgetComponent>(this, FName(*("Widget_" + FString::FromInt(StageWidgets.Num()))));

        Widget->Navigation = Nav;
        Widget->SetWidgetClass(Nav.WidgetClass);
        Widget->SetBlendMode(
            BlendMode == ESphereBlendMode::Translucent 
            ? EWidgetBlendMode::Transparent :
              EWidgetBlendMode::Masked);

        PanoramicSphere->AttachWidget(Widget, Nav.WidgetPosition, Nav.WidgetScale);

        StageWidgets.Add(Widget);
    }
}

void APanoramicDirector::ClearStageWidgets()
{
    for (auto Widget : StageWidgets)
    {
        Widget->DestroyComponent();
    }

    StageWidgets.Empty();
}

bool APanoramicDirector::IsValidStage(const UPanoramicStage& InStage)
{
    if (InStage.MediaType == EPanoramicMediaType::Movie)
    {
        if (MediaPlayer)
        {
            bool CanPlay = MediaPlayer->CanPlaySource(InStage.MovieData.PanoramicMediaSource);
            if (!CanPlay)
            {
                UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("The stage '%s' doesn't have valid movie source."), *InStage.Name);
            }

            return CanPlay;
        }
        else
        {
            UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("The stage '%s' has a media source but the PanoramicDirector"
                " doesn't have a valid MediaPlayer to play it."), *InStage.Name);
            return false;
        }
    }

    check(InStage.MediaType == EPanoramicMediaType::Image);

    bool IsValidTexture = InStage.ImageData.PanoramicTexture != nullptr;
    if (!IsValidTexture)
    {
        UE_LOG(LogStereoPanoramicPlayer, Error, TEXT("The stage '%s' doesn't have valid Texture."), *InStage.Name);
    }

    return IsValidTexture;
}
