// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayTagContainer.h"
#include "Components/PawnComponent.h"
#include "Engine/EngineTypes.h"

#include "ActBattleComponent.generated.h"

#define UE_API ACTGAME_API

class UActAbilitySystemComponent;
class AActor;
/**
 * Component that centralizes battle-related state and routing.
 * Responsibilities:
 * - Lock-on request state.
 * - Lock-on target selection/replication.
 *
 * Deliberately does not own temporary movement/rotation warp tasks. Those belong to CharacterMovement
 * so they can stay as low-level execution primitives instead of battle-layer semantics.
 */
UCLASS(MinimalAPI, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UActBattleComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	
	UE_API UActBattleComponent(const FObjectInitializer& ObjectInitializer);
	
	void InitializeWithAbilitySystem(UActAbilitySystemComponent* InASC);
	void UninitializeFromAbilitySystem();

	/** Unified entry point for lock-on acquisition, routes to server/client internally. */
	UFUNCTION(BlueprintCallable, Category = "Act|Battle")
	UE_API void StartLockOnTarget();

	/** Clear the lock-on target (e.g. on input release or target invalid). */
	UFUNCTION(BlueprintCallable, Category = "Act|Battle")
	UE_API void StopLockOnTarget();

	/** Returns a valid target or clears the lock-on if invalid/out of range. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|Battle")
	UE_API AActor* GetLockOnTarget() const;

	UFUNCTION(BlueprintPure, Category = "Act|Battle")
	bool IsLockOnRequested() const { return bLockOnRequested; }

	UFUNCTION(BlueprintPure, Category = "Act|Battle")
	bool HasLockOnTarget() const { return CurrentLockOnTarget != nullptr; }
	
	void OnFloatingTagChanged(const FGameplayTag Tag, int32 NewCount);

protected:
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	bool ShouldEvaluateLockOn() const;
	void UpdateLockOnTickState();
	void RefreshLockOnTarget();
	AActor* FindLockOnTarget() const;
	AActor* FindLockOnTargetByCameraTrace() const;
	AActor* FindLockOnTargetByProximity() const;

	/** Shared combat-target validity for queries that consume hostile/lockable targets. */
	bool IsValidBattleTarget(const AActor* Target) const;
	bool IsTargetInRange(const AActor* Target, float MaxDistance) const;
	bool ShouldClearLockOnTarget(const AActor* Target) const;

	void SetLockOnRequested(bool bRequested);
	void SetLockOnTarget(AActor* NewTarget);
	/** Notify character movement about lock-on strafe state changes. */
	void NotifyLockOnStrafeState(bool bRequested);

	UFUNCTION(Server, Reliable)
	void ServerSetLockOnRequested(bool bRequested);

	UFUNCTION()
	void OnRep_LockOnTarget(AActor* OldTarget);
	
protected:
	
	UPROPERTY()
	TObjectPtr<UActAbilitySystemComponent> AbilitySystemComponent;

private:
	
	/** Collision channel used for lock-on queries (trace + overlap). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Act|Battle", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TargetCollisionChannel;

	/** Max distance to acquire/keep a lock-on target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Act|Battle", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float LockOnRange;

	/** Search cadence for reacquiring / validating lock-on targets while the request is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Act|Battle", meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
	float LockOnRefreshInterval;

	UPROPERTY(ReplicatedUsing = OnRep_LockOnTarget)
	TObjectPtr<AActor> CurrentLockOnTarget;

	bool bLockOnRequested = false;
};

#undef UE_API
