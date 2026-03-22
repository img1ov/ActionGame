// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "GameplayTagContainer.h"
#include "BulletSystemTypes.h"

#include "ActBulletComponent.generated.h"

class UBulletSystemComponent;
class USkeletalMeshComponent;

/**
 * UActBulletComponent
 *
 * Game-side glue between AnimNotifies / GAS and the BulletSystem plugin.
 *
 * Key responsibilities:
 * - Provide a single entry point for bullet-related AnimNotifies.
 * - Split behavior by network role:
 *   - Simulated proxy: spawn bullets locally for visuals (with collision enabled for hit VFX).
 *   - Authority/autonomous: send gameplay events to GA (GA injects payload and spawns authoritative bullets).
 * - Store runtime bullet InstanceId in a small slot map so later notifies (ProcessManualHits/Destroy) can address
 *   the correct instance deterministically.
 */
UCLASS(ClassGroup = (Act), meta = (BlueprintSpawnableComponent))
class ACTGAME_API UActBulletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActBulletComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// AnimNotify entry points (minimal surface; AnimNotify should only forward).
	UFUNCTION(BlueprintCallable, Category = "Act|Bullet|Notify")
	void Notify_SpawnBullet(USkeletalMeshComponent* MeshComp, FName BulletID, int32 SlotIndex, FName SpawnSocketName, FGameplayTag SpawnEventTag, bool bForceCollisionOnSimProxy = true);

	UFUNCTION(BlueprintCallable, Category = "Act|Bullet|Notify")
	void Notify_ProcessManualHits(int32 SlotIndex, FGameplayTag EventTag, bool bResetHitActorsBefore, bool bApplyCollisionResponse);

	UFUNCTION(BlueprintCallable, Category = "Act|Bullet|Notify")
	void Notify_DestroyBullet(int32 SlotIndex, FGameplayTag EventTag, EBulletDestroyReason Reason, bool bSpawnChildren);

	// NotifyState entry points. NotifyId is editor-stable (generated per notify instance) so GA can map begin/end.
	UFUNCTION(BlueprintCallable, Category = "Act|Bullet|Notify")
	void NotifyState_BeginSpawnBullet(USkeletalMeshComponent* MeshComp, FName BulletID, FName SpawnSocketName, FName NotifyId, FGameplayTag SpawnEventTag, bool bForceCollisionOnSimProxy = true);

	UFUNCTION(BlueprintCallable, Category = "Act|Bullet|Notify")
	void NotifyState_EndDestroyBullet(FName NotifyId, FGameplayTag EndEventTag, EBulletDestroyReason Reason, bool bSpawnChildren);

	// GA helpers: store authoritative/predicted InstanceId for later notifies.
	UFUNCTION(BlueprintCallable, Category = "Act|Bullet|Runtime")
	void SetInstanceIdForSlot(int32 SlotIndex, int32 InstanceId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|Bullet|Runtime")
	int32 GetInstanceIdForSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Act|Bullet|Runtime")
	bool ClearSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Act|Bullet|Runtime")
	void SetInstanceIdForNotifyId(FName NotifyId, int32 InstanceId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|Bullet|Runtime")
	int32 GetInstanceIdForNotifyId(FName NotifyId) const;

	UFUNCTION(BlueprintCallable, Category = "Act|Bullet|Runtime")
	bool ClearNotifyId(FName NotifyId);

private:
	UBulletSystemComponent* FindBulletSystemComponent() const;

	bool IsSimulatedProxy() const;
	bool ShouldRouteToGA() const;

	FTransform BuildSpawnTransform(USkeletalMeshComponent* MeshComp, FName SpawnSocketName) const;

	void SendBulletGameplayEvent(
		FGameplayTag EventTag,
		int32 SlotIndex,
		FName BulletID,
		FName NotifyId,
		bool bResetHitActorsBefore,
		bool bApplyCollisionResponse,
		EBulletDestroyReason DestroyReason,
		bool bSpawnChildren) const;

	// SlotIndex -> bullet instance id (used by AN_* notifies).
	UPROPERTY(Transient)
	TMap<int32, int32> SlotToInstanceId;

	// NotifyId -> bullet instance id (used by ANS_* notify states).
	UPROPERTY(Transient)
	TMap<FName, int32> NotifyIdToInstanceId;
};
