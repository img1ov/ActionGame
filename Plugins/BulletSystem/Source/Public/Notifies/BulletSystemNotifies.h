#pragma once

// BulletSystem: BulletSystemNotifies.h
// Native AnimNotifies used to integrate BulletSystem with montage notifies + GAS gameplay events.

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"

#include "BulletSystemTypes.h"

#include "BulletSystemNotifies.generated.h"

class USkeletalMeshComponent;

/**
 * Payload carried via GameplayEventData.OptionalObject.
 * Used by GA to read notify parameters (BulletID/Alias/Process/Destroy parameters).
 */
UCLASS(BlueprintType)
class BULLETGAME_API UBulletSystemNotifyPayload : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	FName BulletID = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	FName InstanceAlias = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	bool bResetHitActorsBefore = true;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	bool bApplyCollisionResponse = true;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	EBulletDestroyReason DestroyReason = EBulletDestroyReason::External;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	bool bSpawnChildren = true;
};

/**
 * Spawn bullet AnimNotify.
 * - SimulatedProxy: spawn locally for visuals (collision enabled for hit VFX).
 * - Authority/Autonomous: send gameplay event to GA (GA injects payload + spawns authoritative bullet).
 */
UCLASS(meta = (DisplayName = "Bullet Spawn"))
class BULLETGAME_API UAN_Bullet_SpawnBullet : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName BulletID = NAME_None;

	/** Runtime alias written into InitParams.InstanceAlias (used for later process/destroy lookups). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName InstanceAlias = NAME_None;

	/** Optional socket to use as spawn transform basis. If None/missing, uses mesh component transform. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName SpawnSocketName = NAME_None;

	/** GameplayEvent tag routed to GA on Authority/Autonomous. If invalid, spawns locally instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag SpawnEventTag;

	/** Simulated proxy uses collision to drive local hit VFX. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|SimProxy")
	bool bForceCollisionOnSimProxy = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/**
 * Process manual hits AnimNotify.
 * Targets the bullet instance by InstanceAlias.
 */
UCLASS(meta = (DisplayName = "Bullet Process Manual Hits"))
class BULLETGAME_API UAN_Bullet_ProcessManualHits : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName InstanceAlias = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	bool bResetHitActorsBefore = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	bool bApplyCollisionResponse = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/**
 * Destroy bullet AnimNotify.
 * Targets the bullet instance by InstanceAlias.
 */
UCLASS(meta = (DisplayName = "Bullet Destroy"))
class BULLETGAME_API UAN_Bullet_DestroyBullet : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName InstanceAlias = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	EBulletDestroyReason Reason = EBulletDestroyReason::External;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	bool bSpawnChildren = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/**
 * Spawn bullet during a window, auto-destroy at NotifyEnd.
 * If InstanceAlias is empty, a unique per-notify NotifyId will be used as the runtime alias.
 */
UCLASS(meta = (DisplayName = "Bullet Spawn (State)"))
class BULLETGAME_API UANS_Bullet_SpawnBullet : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName BulletID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName InstanceAlias = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag SpawnEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag EndEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|End")
	EBulletDestroyReason EndReason = EBulletDestroyReason::External;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|End")
	bool bSpawnChildrenOnEnd = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|SimProxy")
	bool bForceCollisionOnSimProxy = true;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

private:
	FName GetEffectiveAlias() const;

	// Unique ID per notify instance for runtime mapping when InstanceAlias is not specified.
	UPROPERTY(VisibleAnywhere, Category = "Bullet")
	FName NotifyId;
};

