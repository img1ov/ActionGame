// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "Character/ActCharacterMovementTypes.h"

#include "AT_ApplyAddMove.generated.h"

class UActCharacterMovementComponent;
class UCurveFloat;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyAddMoveDelegate);

/**
 * GAS task wrapper for one authored translation AddMove.
 *
 * Primary use:
 * - pure procedural authored motion such as knockback, launch, dash, attack push and other
 *   explicit gameplay displacement
 *
 * Do not use this task to "follow a montage" when the authored motion actually lives in
 * animation root motion. That path belongs to PlayAddMoveMontageAndWaitForEvent.
 */
UCLASS()
class ACTGAME_API UAT_ApplyAddMove : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAT_ApplyAddMove(const FObjectInitializer& ObjectInitializer);

	/** Fired when a finite-duration AddMove reaches its authored end time. */
	UPROPERTY(BlueprintAssignable)
	FApplyAddMoveDelegate OnFinish;

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (DisplayName = "ApplyAddMove", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAT_ApplyAddMove* ApplyAddMove(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		EActAddMoveSpace Space,
		FVector Direction,
		float Strength,
		float Duration,
		UCurveFloat* StrengthOverTime = nullptr,
		USkeletalMeshComponent* Mesh = nullptr);

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual FString GetDebugString() const override;

private:
	/** Resolves the custom movement component from the current avatar actor. */
	UActCharacterMovementComponent* GetActMovementComponent() const;
	/** Returns the movement basis mesh when Space == Mesh. */
	USkeletalMeshComponent* ResolveMeshBasis() const;
	/** Creates one authored AddMove entry on the movement component and stores its local handle. */
	bool ApplyAuthoredAddMove();
	/** Infinite AddMove stays alive until gameplay stops it explicitly. */
	bool HasInfiniteDuration() const;
	/** Finite AddMove completion callback used to broadcast OnFinish and end the task. */
	void OnTimeFinish();
	/** Non-local abilities need a stable SyncId so owner/server/simulated proxy align to the same AddMove. */
	bool ShouldUseNetworkSyncId() const;
	int32 GetOrCreateAddMoveSyncId() const;

private:
	/** Coordinate space used to interpret Direction. */
	UPROPERTY()
	EActAddMoveSpace Space = EActAddMoveSpace::World;

	/** Authored motion direction, normalized at task creation time. */
	UPROPERTY()
	FVector Direction = FVector::ForwardVector;

	/** Authored motion magnitude in units per second. */
	UPROPERTY()
	float Strength = 0.0f;

	/** Authored duration in seconds; < 0 means persistent until stopped. */
	UPROPERTY()
	float Duration = 0.0f;

	/** Optional scalar profile sampled over normalized AddMove lifetime. */
	UPROPERTY()
	TObjectPtr<UCurveFloat> StrengthOverTime = nullptr;

	/** Optional mesh-space basis when Space == Mesh. */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

	/** Local runtime handle returned by CMC. */
	int32 AddMoveHandle = INDEX_NONE;
	/** Stable runtime identity shared across prediction/server/bootstrap when needed. */
	mutable int32 AddMoveSyncId = INDEX_NONE;

	/** Finite AddMove completion timer. */
	FTimerHandle FinishTimerHandle;
};
