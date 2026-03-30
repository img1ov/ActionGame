// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "Character/ActCharacterMovementTypes.h"

#include "AT_PlayAddMoveMontageAndWaitForEvent.generated.h"

class UActAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayAddMoveMontageAndWaitForEventDelegate, FGameplayTag, EventTag, FGameplayEventData, EventData);

UENUM(BlueprintType)
enum class EActAddMoveMontageRangeMode : uint8
{
	CurrentSection,
	ThroughSection,
	EntireMontage,
};

USTRUCT(BlueprintType)
struct FActAddMoveMontageSettings
{
	GENERATED_BODY()

	/** Chooses whether this montage is pure presentation, extracted into AddMove, or left as native root motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Add Move")
	EActMontageExecutionMode ExecutionMode = EActMontageExecutionMode::ExtractedRootMotion;

	/** Range of montage root motion to extract when ExecutionMode == ExtractedRootMotion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Add Move")
	EActAddMoveMontageRangeMode RangeMode = EActAddMoveMontageRangeMode::CurrentSection;

	/** Optional terminating section for multi-section extraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Add Move", meta = (EditCondition = "RangeMode == EActAddMoveMontageRangeMode::ThroughSection"))
	FName EndSectionName = NAME_None;

	/** Basis used to convert extracted root motion into world-space displacement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Add Move")
	EActMotionBasisMode BasisMode = EActMotionBasisMode::MeshStartFrozen;

	/** Whether extracted root-motion rotation should also become AddMove rotation delta. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Add Move")
	bool bApplyRotation = false;

	/** Whether explicit gameplay rotation AddMove should win over extracted root-motion yaw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Add Move")
	bool bRespectAddMoveRotation = true;

	/** Whether extracted vertical translation should be discarded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Add Move")
	bool bIgnoreZAccumulate = true;

	/** Whether locomotion velocity should be cleared before the first extracted AddMove segment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Add Move")
	bool bBrakeMovementOnStart = true;

	bool UsesExtractedRootMotion() const
	{
		return ExecutionMode == EActMontageExecutionMode::ExtractedRootMotion;
	}

	bool UsesProceduralPresentation() const
	{
		return ExecutionMode == EActMontageExecutionMode::Procedural;
	}

	bool UsesNativeRootMotion() const
	{
		return ExecutionMode == EActMontageExecutionMode::NativeRootMotion;
	}
};

/**
 * Specialized montage task for "montage plays for visuals/events, authored root motion is extracted into AddMove".
 *
 * Primary use:
 * - the montage really contains authored root motion you want CharacterMovement to execute through
 *   the AddMove framework for prediction/networking
 *
 * Do not use this as the default attack / hit-react task when displacement is already authored as
 * pure procedural AddMove in gameplay. In that case use:
 * - ApplyAddMove
 * - PlayMontageAndWaitForEvent
 */
UCLASS()
class ACTGAME_API UAT_PlayAddMoveMontageAndWaitForEvent : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAT_PlayAddMoveMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (DisplayName = "PlayAddMoveMontageAndWaitForEvent", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAT_PlayAddMoveMontageAndWaitForEvent* CreatePlayAddMoveMontageAndWaitForEvent(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		UAnimMontage* MontageToPlay,
		FGameplayTagContainer EventTags,
		float Rate = 1.f,
		FName StartSection = NAME_None,
		bool bStopWhenAbilityEnds = true,
		FActAddMoveMontageSettings AddMoveSettings = FActAddMoveMontageSettings());

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	bool StopPlayingMontage();
	/** Refreshes extracted AddMove when montage advances into a new section. */
	bool RefreshPredictedAddMoveForSection(const FName SectionName);
	/** Reads the montage section currently active on the owning AnimInstance. */
	FName GetCurrentMontageSectionName() const;
	/** Stable SyncId used so extracted AddMove aligns across prediction/server/bootstrap. */
	int32 GetOrCreateAddMoveSyncId() const;
	UActAbilitySystemComponent* GetActAbilitySystemComponent();

	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void OnAbilityCancelled();
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

private:
	/** Bound to AnimInstance so task delegates mirror montage lifecycle. */
	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
	/** Handles removal of ability-cancel and gameplay-event callbacks on task destroy. */
	FDelegateHandle CancelledHandle;
	FDelegateHandle EventHandle;

public:
	UPROPERTY(BlueprintAssignable)
	FPlayAddMoveMontageAndWaitForEventDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FPlayAddMoveMontageAndWaitForEventDelegate OnBlendOut;

	UPROPERTY(BlueprintAssignable)
	FPlayAddMoveMontageAndWaitForEventDelegate OnInterrupted;

	UPROPERTY(BlueprintAssignable)
	FPlayAddMoveMontageAndWaitForEventDelegate OnCancelled;

	UPROPERTY(BlueprintAssignable)
	FPlayAddMoveMontageAndWaitForEventDelegate EventReceived;

private:
	UPROPERTY()
	UAnimMontage* MontageToPlay = nullptr;

	UPROPERTY()
	FGameplayTagContainer EventTags;

	UPROPERTY()
	float Rate = 1.0f;

	UPROPERTY()
	FName StartSection = NAME_None;

	UPROPERTY()
	bool bStopWhenAbilityEnds = true;

	UPROPERTY()
	FActAddMoveMontageSettings AddMoveSettings;

	/** Local runtime handle for the extracted AddMove entry owned by this task. */
	int32 AddMoveHandle = INDEX_NONE;
	/** True once native montage root motion has been disabled for procedural ownership. */
	bool bPushedDisableRootMotion = false;
	/** Guards section refresh so each section is extracted only once. */
	FName LastAppliedAddMoveSection = NAME_None;
	/** Stable identity shared with owner/server/simulated-proxy bootstrap. */
	mutable int32 AddMoveSyncId = INDEX_NONE;
};
