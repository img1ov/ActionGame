// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "Character/ActCharacterMovementTypes.h"

#include "AT_ApplySetAddMove.generated.h"

class UActCharacterMovementComponent;
class UCurveFloat;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplySetAddMoveDelegate);

/**
 * Bridges GAS task authoring style with the project's AddMove movement framework.
 *
 * This task intentionally mirrors the high-level ergonomics of GAS ApplyRootMotion tasks:
 * - authored from a gameplay ability
 * - starts one timed movement effect immediately
 * - optionally scales over time with a float curve
 * - automatically cleans up when the task or ability ends
 *
 * Unlike GAS root motion tasks, the actual movement is executed through
 * UActCharacterMovementComponent::SetAddMove*, so it stays within the project's
 * explicit procedural movement framework.
 *
 * Intended use:
 * - Owner-local responsive movement that should live in the project's predictive movement layer.
 *
 * Not the primary path for:
 * - Pure server-authoritative movement presentation that can rely on UE/GAS native montage/root motion.
 */
UCLASS()
class ACTGAME_API UAT_ApplySetAddMove : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** Initializes ticking so finite-duration moves can broadcast OnFinish and self-end. */
	UAT_ApplySetAddMove(const FObjectInitializer& ObjectInitializer);

	/** Broadcast when a finite-duration AddMove reaches its authored end time. */
	UPROPERTY(BlueprintAssignable)
	FApplySetAddMoveDelegate OnFinish;

	/**
	 * Applies one procedural AddMove from a gameplay ability.
	 *
	 * Direction is interpreted in the selected Space:
	 * - World: already world-space
	 * - Actor: local to the updated component / capsule
	 * - Mesh: local to the provided mesh basis
	 */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (DisplayName = "ApplySetAddMove", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAT_ApplySetAddMove* ApplySetAddMove(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		EActAddMoveSpace Space,
		FVector Direction,
		float Strength,
		float Duration,
		UCurveFloat* StrengthOverTime = nullptr,
		USkeletalMeshComponent* Mesh = nullptr);

	/** Creates the authored AddMove on the movement component. */
	virtual void Activate() override;

	/** Finite-duration moves finish themselves once the authored time elapses. */
	virtual void TickTask(float DeltaTime) override;

	/** Cancels the owned AddMove immediately. */
	virtual void ExternalCancel() override;

	/** Releases the owned AddMove handle when the task ends or ability is destroyed. */
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/** Debug string shown by GAS task tooling. */
	virtual FString GetDebugString() const override;

private:
	/** Resolves the custom movement component from the ability avatar. */
	UActCharacterMovementComponent* GetActMovementComponent() const;

	/** Returns the movement basis mesh when Space == Mesh. */
	USkeletalMeshComponent* ResolveMeshBasis() const;

	/** Applies the AddMove to the movement component and stores the returned handle. */
	bool ApplyAuthoredAddMove();

	/** Returns true when Duration means the move should persist until explicitly stopped. */
	bool HasInfiniteDuration() const;

	/** Called by the world timer once the finite-duration authored move has reached its end. */
	void OnTimeFinish();

private:
	/** Selected coordinate space used to interpret Direction. */
	UPROPERTY()
	EActAddMoveSpace Space = EActAddMoveSpace::World;

	/** Direction component of the authored extra movement. */
	UPROPERTY()
	FVector Direction = FVector::ForwardVector;

	/** Magnitude component of the authored extra movement, in units/sec. */
	UPROPERTY()
	float Strength = 0.0f;

	/**
	 * Authored duration in seconds.
	 * - > 0: finite duration
	 * - < 0: infinite duration until task/ability cleanup
	 */
	UPROPERTY()
	float Duration = 0.0f;

	/** Optional scalar curve sampled by AddMove over time. */
	UPROPERTY()
	TObjectPtr<UCurveFloat> StrengthOverTime = nullptr;

	/** Optional mesh basis used when Space == Mesh. */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

	/** Local AddMove handle returned by the movement component. */
	int32 AddMoveHandle = INDEX_NONE;

	/** World time when the finite-duration AddMove started. */
	float StartTimeSeconds = 0.0f;

	/** Timer used to broadcast OnFinish for finite-duration AddMove tasks. */
	FTimerHandle FinishTimerHandle;
};
