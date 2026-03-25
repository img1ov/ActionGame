#pragma once

// BulletSystem: BulletSystemComponent.h
// Entry-point glue for gameplay code/blueprints. Bullet simulation remains world-scoped
// (UBulletWorldSubsystem/UBulletController).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BulletSystemTypes.h"

#include "BulletSystemComponent.generated.h"

class UBulletConfig;

/**
 * Per-component registry for resolving runtime bullet instance ids by a lightweight key.
 *
 * Purpose:
 * - AnimNotifies on simulated proxies do not run GA, but still need a stable way to find the spawned bullet instance.
 * - Gameplay code may also want to "name" a spawned bullet instance for later operations (process hit / destroy).
 *
 * Notes:
 * - Key is a local runtime label. It does not replicate.
 * - Same key may have multiple active instances during overlapping windows (combo notifies, buffering, etc.).
 *   GetInstanceIdByKey returns the most recently spawned valid instance.
 */
USTRUCT()
struct FBulletInstanceRegistry
{
	GENERATED_BODY()

	/** Key -> InstanceIds (in spawn order). Runtime-only, not serialized. */
	TMap<FName, TArray<int32>> InstanceKeyMap;

	void ClearKeyMap() { InstanceKeyMap.Reset(); }
	bool RemoveKey(FName Key) { return !Key.IsNone() && InstanceKeyMap.Remove(Key) > 0; }
	void AddToKeyMap(FName Key, int32 InstanceId)
	{
		if (Key.IsNone())
		{
			return;
		}
		if (InstanceId == INDEX_NONE)
		{
			InstanceKeyMap.Remove(Key);
			return;
		}
		InstanceKeyMap.FindOrAdd(Key).Add(InstanceId);
	}
	
	int32 GetInstanceIdByKey(FName Key) const
	{
		if (Key.IsNone())
		{
			return INDEX_NONE;
		}
		if (const TArray<int32>* Found = InstanceKeyMap.Find(Key))
		{
			return Found->Num() > 0 ? Found->Last() : INDEX_NONE;
		}
		return INDEX_NONE;
	}
};

UCLASS(ClassGroup = (BulletSystem), meta = (BlueprintSpawnableComponent))
class BULLETSYSTEM_API UBulletSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBulletSystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Inject config from game-side (e.g. HeroComponent reads PawnData and forwards BulletConfig here).
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	void SetBulletConfig(UBulletConfig* InConfig);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem")
	UBulletConfig* GetBulletConfig() const { return BulletConfig; }

	/** Resolve a runtime bullet instance by key. Invalid entries are pruned. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|InstanceRegistry")
	int32 GetInstanceIdByKey(FName InstanceKey) const;

	/**
	 * Consume the oldest valid instance id for a key (and remove it from the registry).
	 * This is useful for notify-state style windows that can overlap: End events should destroy the oldest still-alive instance for that key.
	 */
	UFUNCTION(BlueprintCallable, Category = "BulletSystem|InstanceRegistry")
	int32 ConsumeOldestInstanceIdByKey(FName InstanceKey);

	/** Set or overwrite key -> InstanceId. Passing INDEX_NONE removes the key. */
	UFUNCTION(BlueprintCallable, Category = "BulletSystem|InstanceRegistry")
	void SetInstanceIdKey(FName InstanceKey, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "BulletSystem|InstanceRegistry")
	bool RemoveInstanceKey(FName InstanceKey);

	UFUNCTION(BlueprintCallable, Category = "BulletSystem|InstanceRegistry")
	void ClearInstanceKeys();

	/** True if key resolves to a live bullet instance. Invalid entries are pruned. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|InstanceRegistry")
	bool IsInstanceKeyValid(FName InstanceKey) const;

	// Local spawn entry.
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	int32 SpawnBullet(FName BulletId, const FBulletInitParams& InitParams);

	// Request destroy for a bullet instance (local).
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	bool DestroyBullet(int32 InstanceId);

	// Manual-hit trigger: process stored overlaps as hits (fires OnHit logic chain, interact, collision response).
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	int32 ProcessManualHits(int32 InstanceId, bool bApplyCollisionResponse);

private:
	int32 SpawnBulletInternal(FName BulletId, const FBulletInitParams& InitParams);
	bool DestroyBulletInternal(int32 InstanceId) const;
	int32 ProcessManualHitsInternal(int32 InstanceId, bool bApplyCollisionResponse) const;
	bool IsInstanceIdValid(int32 InstanceId) const;

	UPROPERTY()
	TObjectPtr<UBulletConfig> BulletConfig;

	UPROPERTY(Transient)
	mutable FBulletInstanceRegistry InstanceRegistry;
};
