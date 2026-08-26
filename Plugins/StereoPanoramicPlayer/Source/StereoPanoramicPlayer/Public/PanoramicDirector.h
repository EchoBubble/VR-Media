// Copyright 2019 UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/StreamableManager.h"

#include "PanoramicSphere.h"

#include "PanoramicDirector.generated.h"

/**
 * Handle the playing of panoramic experiences.
 */
UCLASS()
class STEREOPANORAMICPLAYER_API APanoramicDirector : public AActor
{
    GENERATED_UCLASS_BODY()

public:

    /** Called when the experience has started */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExperienceStarted);

    /** Called when the experience has terminated */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExperienceTerminated);

    /** Called when the stage has changed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStageChanged, class UPanoramicStage*, NewStage);

    /** Called when the experience has started */
    UPROPERTY(BlueprintAssignable)
    FOnExperienceStarted OnExperienceStarted;

    /** Called when the experience has terminated */
    UPROPERTY(BlueprintAssignable)
    FOnExperienceTerminated OnExperienceTerminated;

    /** Called when the stage has changed */
    UPROPERTY(BlueprintAssignable)
    FOnStageChanged OnStageChanged;

    /** The blend mode of the sphere (used just when the sphere is spawned by the director) */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer|Sphere")
    ESphereBlendMode BlendMode;

public:

    /** Enters into panoramic mode playing the given experience */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer")
    void Play(class UPanoramicExperience* Experience, float StartingYawRotation = 0.f);

    /** Exits from panoramic mode */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer")
    void End();

    /** Returns true if the experience is playing, false otherwise */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer")
    bool IsPlaying() const;

    /** Move to the given stage or exit from the experience if InNextStage is null. */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer")
    void MoveOrExit(TSoftObjectPtr<class UPanoramicStage> InNextStage);

    /** Move through the given stage navigation. */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer")
    void MoveThrough(const struct FPanoramicStageNavigation& Navigation);

    /** Returns the current stage */
    UFUNCTION(BlueprintCallable, Category = "StereoPanoramicPlayer")
    class UPanoramicStage* GetCurrentStage() const;

protected:

    //~ AActor interface

    /** Override AActor::BeginPlay */
    virtual void BeginPlay() override;

    /** Override AActor::EndPlay */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Override AActor::Tick */
    virtual void Tick(float DeltaSeconds) override;

    //~ End AActor interface

private:

    bool CanMove() const;
    bool ScheduleToMove(TSoftObjectPtr<class UPanoramicStage> InNextStage);
    bool DoMove();
    void DoExit();

private:

    UFUNCTION()
    void OnStageLoaded();

private:

    static void ForceTextureStreamInIfNeeded(UTexture2D* InTexture);
    static bool IsTextureReadyForRendering(UTexture2D* InTexture);

private:

    void EnterInSpectatorMode();
    void ExitFromSpectatorMode();

private:

    void CreateStageWidgets();
    void ClearStageWidgets();

private:

    bool IsValidStage(const UPanoramicStage& InStage);

private:

    void FadeIn();
    void FadeOut();

    float GetFadeValue() const;
    float GetCutOffValue() const;
    float GetCutOffAlphaValue() const;
    void UpdateFading(float DeltaSeconds);

    bool HasNextStageLoadCompleted() const;

    float GetTransitionDuration() const;

    void OnFadeInEnd();
    void OnFadeOutEnd();

    UFUNCTION()
    void OnMediaEndReached();

    bool EnterOrExitTransition() const;

public:

    /** An optional sphere used for rendering, 
        if null a default one will be created automatically */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StereoPanoramicPlayer|Sphere")
    class APanoramicSphere* PanoramicSphere;

    /** A sound component to play sounds */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "StereoPanoramicPlayer|Media")
    class UMediaSoundComponent* MediaSoundComponent;

    /** The component used to perfom interactions with the stage navigations */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    class UPanoramicInteractionComponent* InteractionComponent;

    /** An optional UMediaPlayer used to play media sources;
        it's mandatory only if the experience has some media sources to play */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Media")
    class UMediaPlayer* MediaPlayer;

    /** An optional UMediaTexture used to play media sources;
        it's mandatory only if the experience has some media sources to play. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Media")
    class UMediaTexture* MediaTexture;

    /** An optional experience to play automatically when the game begins. 
        Note: you can play an experience whenever you want through the Play method. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StereoPanoramicPlayer")
    class UPanoramicExperience* Experience;

    /** Defines if the fade effect is on when we enter/exit into/from the experience */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Fading")
    bool FadeOnEnterAndExit;

    /** Defines if the fade effect is on when we change stage */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Fading")
    bool FadeOnTransition;

    /** The duration (in seconds) of the fading effect. Put to zero to disable it. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Fading")
    float FadeDuration;

    /** An optional UCurveFloat used to customize the fade transition.
    Values should be in the range [0,1]. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Fading")
    class UCurveFloat* FadeCurve;

    /** An optional background texture used for the crossfading */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Fading")
    UTexture2D* BackgroundTexture;

    /** Defines if the cutoff effect is on when we enter/exit into/from the experience */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Cutoff")
    bool CutOffOnEnterAndExit;

    /** Defines if the cutoff effect is on when we change stage */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Cutoff")
    bool CutOffOnTransition;

    /** The duration (in seconds) of the cutoff effect. Put to zero to disable it. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Cutoff")
    float CutOffDuration;

    /** An optional texture used to customize the cutoff transition. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Cutoff")
    UTexture2D* CutOffTexture;

    /** An optional UCurveFloat used to customize the cutoff transition.
    Values should be in the range [0,1]. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Cutoff")
    class UCurveFloat* CutOffCurve;

    /** An optional UCurveFloat used to customize the alpha value of the cutoff when
    we enter/exit into the panoramic experience.
    Values should be in the range [0,1]. 
    If not set, the alpha value for the cutoff part is fully transparent.*/
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "StereoPanoramicPlayer|Cutoff")
    class UCurveFloat* CutOffAlphaCurve;

private:

    UPROPERTY(Transient)
    TArray<class UWidgetComponent*> StageWidgets;

    UPROPERTY(Transient)
    class UPanoramicStage* CurrentStage;

    UPROPERTY(Transient)
    TSoftObjectPtr<class UPanoramicStage> PrevStage;

    UPROPERTY(Transient)
    TSoftObjectPtr<class UPanoramicStage> NextStage;

    TSharedPtr<struct FStreamableHandle> LoadingStageHandle;

    FStreamableManager AssetLoader;

    float TransitionTime;

    enum class EFadeStatus : uint8
    {
        NO_FADING = 0,
        FADING_IN,
        FADING_OUT
    };

    EFadeStatus FadeStatus;

    float FrameYawRotation;
    TOptional<float> YawRotationFromNavigation;

    FRotator InitialPawnRotation;
};
