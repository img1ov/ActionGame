// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/PawnComponent.h"
#include "Engine/EngineTypes.h"
#include "NativeGameplayTags.h"

#include "ActBattleComponent.generated.h"

#define UE_API ACTGAME_API

class AActor;

// Tag used by strafe-move style abilities (e.g. hold-to-lock-on).
ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Movement_StrafeMove);
/**
 * Component that centralizes battle-related state and routing.
 * Lock-on selection/replication is handled here to keep gameplay abilities modular.
 */
UCLASS(MinimalAPI, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UActBattleComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	
	UE_API UActBattleComponent(const FObjectInitializer& ObjectInitializer);

	/** Unified entry point for lock-on acquisition, routes to server/client internally. */
	UFUNCTION(BlueprintCallable, Category = "Act|Battle")
	UE_API void StartLockOnTarget();

	/** Clear the lock-on target (e.g. on input release or target invalid). */
	UFUNCTION(BlueprintCallable, Category = "Act|Battle")
	UE_API void StopLockOnTarget();

	/** Returns a valid target or clears the lock-on if invalid/out of range. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|Battle")
	UE_API AActor* GetLockOnTarget();

	UFUNCTION(BlueprintPure, Category = "Act|Battle")
	bool IsLockOnActive() const { return CurrentLockOnTarget != nullptr; }

protected:
	
	virtual void OnRegister() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	AActor* FindLockOnTarget() const;
	AActor* FindLockOnTargetByCameraTrace() const;
	AActor* FindLockOnTargetByProximity() const;

	bool IsValidLockOnTarget(const AActor* Target) const;
	bool IsTargetInRange(const AActor* Target, float MaxDistance) const;
	bool ShouldClearLockOnTarget(const AActor* Target) const;

	void SetLockOnTarget(AActor* NewTarget);

	UFUNCTION(Server, Reliable)
	void ServerSetLockOnTarget(AActor* NewTarget);

	UFUNCTION()
	void OnRep_LockOnTarget(AActor* OldTarget);

private:
	
	/** Collision channel used for lock-on queries (trace + overlap). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Act|Battle", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TargetCollisionChannel;

	/** Max distance to acquire/keep a lock-on target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Act|Battle", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	float LockOnRange;

	UPROPERTY(ReplicatedUsing = OnRep_LockOnTarget)
	TObjectPtr<AActor> CurrentLockOnTarget;
};

#undef UE_API
