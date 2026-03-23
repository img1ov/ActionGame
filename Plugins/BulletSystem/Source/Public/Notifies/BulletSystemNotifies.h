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
 * Used by GA to read notify parameters (BulletId/InitParams/Process/Destroy parameters).
 */
UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletNotifyOptionalObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	FName BulletId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	FBulletInitParams InitParams;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	FName InstanceKey = NAME_None;

	// ProcessManualHits parameters.
	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	bool bResetHitActorsBefore = true;
};

/**
 * Spawn bullet AnimNotify.
 * - SimulatedProxy: spawn locally for visuals (collision enabled for hit VFX).
 * - Authority/Autonomous: send gameplay event to GA (GA injects payload + spawns authoritative bullet).
 */
UCLASS(meta = (DisplayName = "Bullet Spawn"))
class BULLETSYSTEM_API UAN_SpawnBullet : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName BulletId = NAME_None;

	/** Runtime key written into InitParams.InstanceKey (used for later process/destroy lookups). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName InstanceKey = NAME_None;

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

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Bullet")
	void OnNotify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const;
};

 /**
  * Process manual hits AnimNotify.
  * Targets the bullet instance by InstanceKey.
  */
UCLASS(meta = (DisplayName = "Bullet Process Manual Hits"))
class BULLETSYSTEM_API UAN_ProcessManualHits : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName InstanceKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	bool bResetHitActorsBefore = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Bullet")
	void OnNotify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const;
};

 /**
  * Destroy bullet AnimNotify.
  * Targets the bullet instance by InstanceKey.
  */
UCLASS(meta = (DisplayName = "Bullet Destroy"))
class BULLETSYSTEM_API UAN_DestroyBullet : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName InstanceKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag EventTag;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Bullet")
	void OnNotify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const;
};

 /**
  * Spawn bullet during a window, auto-destroy at NotifyEnd.
  * If InstanceKey is empty, BulletId will be used as the runtime key (author explicit keys for overlapping windows).
  */
 UCLASS(meta = (DisplayName = "Bullet Spawn (State)"))
class BULLETSYSTEM_API UANS_SpawnBullet : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName BulletId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName InstanceKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag SpawnEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag EndEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|SimProxy")
	bool bForceCollisionOnSimProxy = true;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Bullet")
	void OnBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Bullet")
	void OnEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const;
};
