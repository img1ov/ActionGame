// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Abilities/Tasks/AbilityTask.h"

#include "AT_PlayMontageAndWaitForEvent.generated.h"

class UActAbilitySystemComponent;
/** Delegate type used, EventTag and Payload may be empty if it came from the montage callbacks */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayMontageAndWaitForEventDelegate, FGameplayTag, EventTag, FGameplayEventData, EventData);

UENUM(BlueprintType)
enum class EActMontageMotionRangeMode : uint8
{
	CurrentSection,
	ThroughSection,
	EntireMontage,
};

/**
 * Minimal task-side configuration for montage-authored AddMove motion.
 * This stays local to the montage task instead of increasing ability-side complexity.
 */
USTRUCT(BlueprintType)
struct FActMontageMotionSettings
{
	GENERATED_BODY()

	/**
	 * Enables montage-authored AddMove extraction for this task instance.
	 * This only applies on machines that actually own and execute this task instance.
	 * Replicated montage playback by itself will not automatically recreate the task-side AddMove.
	 * Use this for locally responsive procedural motion. For pure server-authoritative montage motion,
	 * UE/GAS native root motion remains the simpler path.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Motion")
	bool bEnabled = false;

	/** Controls which authored montage range is converted into AddMove motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Motion", meta = (EditCondition = "bEnabled"))
	EActMontageMotionRangeMode RangeMode = EActMontageMotionRangeMode::CurrentSection;

	/** Used when RangeMode is ThroughSection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Motion", meta = (EditCondition = "bEnabled && RangeMode == EActMontageMotionRangeMode::ThroughSection"))
	FName EndSectionName = NAME_None;

	/** Applies authored rotation together with extracted translation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Motion", meta = (EditCondition = "bEnabled"))
	bool bApplyRotation = true;

	/** Discards extracted vertical translation so authored montage motion stays planar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Motion", meta = (EditCondition = "bEnabled"))
	bool bIgnoreZAccumulate = true;

	/** Clears locomotion velocity when the first extracted motion segment starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Motion", meta = (EditCondition = "bEnabled"))
	bool bBrakeMovementOnStart = true;
};

/**
 * UAT_PlayMontageAndWaitForEvent
 * This task combines PlayMontageAndWait and WaitForEvent into one task, so you can wait for multiple types of activations such as from a melee combo
 * Much of this code is copied from one of those two ability tasks
 * This is a good task to look at as an example when creating game-specific tasks
 * It is expected that each game will have a set of game-specific tasks to do what they want
 */
UCLASS()
class ACTGAME_API UAT_PlayMontageAndWaitForEvent : public UAbilityTask
{
	GENERATED_BODY()

public:

	UAT_PlayMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer);

	/**
	 * Play a montage and wait for it end. If a gameplay event happens that matches EventTags (or EventTags is empty), the EventReceived delegate will fire with a tag and event data.
	 * If StopWhenAbilityEnds is true, this montage will be aborted if the ability ends normally. It is always stopped when the ability is explicitly cancelled.
	 * On normal execution, OnBlendOut is called when the montage is blending out, and OnCompleted when it is completely done playing
	 * OnInterrupted is called if another montage overwrites this, and OnCancelled is called if the ability or task is cancelled
	 *
	 * @param TaskInstanceName Set to override the name of this task, for later querying
	 * @param MontageToPlay The montage to play on the character
	 * @param EventTags Any gameplay events matching this tag will activate the EventReceived callback. If empty, all events will trigger callback
	 * @param Rate Change to play the montage faster or slower
	 * @param bStopWhenAbilityEnds If true, this montage will be aborted if the ability ends normally. It is always stopped when the ability is explicitly cancelled
	 * @param AnimRootMotionTranslationScale Change to modify size of root motion or set to 0 to block it entirely
	 */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (DisplayName = "PlayMontageAndWaitForEvent", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAT_PlayMontageAndWaitForEvent* CreatePlayMontageAndWaitForEvent(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		UAnimMontage* MontageToPlay,
		FGameplayTagContainer EventTags,
		float Rate = 1.f,
		FName StartSection = NAME_None,
		bool bStopWhenAbilityEnds = true,
		float AnimRootMotionTranslationScale = 1.f,
		FActMontageMotionSettings MontageMotionSettings = FActMontageMotionSettings()
	);
	
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:

	/** Checks if the ability is playing a montage and stops that montage, returns true if a montage was stopped, false if not. */
	bool StopPlayingMontage();
	bool RefreshPredictedMotionForSection(const FName SectionName);
	FName GetCurrentMontageSectionName() const;
	int32 GetOrCreateMotionSyncId() const;

	/** Returns our ability system component */
	UActAbilitySystemComponent* GetActAbilitySystemComponent();

	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void OnAbilityCancelled();
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
	FDelegateHandle CancelledHandle;
	FDelegateHandle EventHandle;

public:

	/** The montage completely finished playing */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate OnCompleted;

	/** The montage started blending out */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate OnBlendOut;

	/** The montage was interrupted */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate OnInterrupted;

	/** The ability task was explicitly cancelled by another ability */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate OnCancelled;

	/** One of the triggering gameplay events happened */
	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitForEventDelegate EventReceived;

private:

	/** Montage that is playing */
	UPROPERTY()
	UAnimMontage* MontageToPlay;

	/** List of tags to match against gameplay events */
	UPROPERTY()
	FGameplayTagContainer EventTags;

	/** Playback rate */
	UPROPERTY()
	float Rate;

	/** Section to start montage from */
	UPROPERTY()
	FName StartSection;

	/** Scales native animation root motion when montage motion extraction is disabled. */
	UPROPERTY()
	float AnimRootMotionTranslationScale;

	/** Rather montage should be aborted if ability ends */
	UPROPERTY()
	bool bStopWhenAbilityEnds;

	UPROPERTY()
	FActMontageMotionSettings MontageMotionSettings;

	int32 AddMoveHandle = INDEX_NONE;
	bool bPushedDisableRootMotion = false;
	FName LastAppliedMotionSection = NAME_None;
	mutable int32 MotionSyncId = INDEX_NONE;
	
};
