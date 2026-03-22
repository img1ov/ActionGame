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
 * Per-component registry for resolving runtime bullet instance ids by a lightweight alias.
 *
 * Purpose:
 * - AnimNotifies on simulated proxies do not run GA, but still need a stable way to find the spawned bullet instance.
 * - Gameplay code may also want to "name" a spawned bullet instance for later operations (process hit / destroy).
 *
 * Notes:
 * - Alias is a local runtime label. It does not replicate.
 * - Same alias overwrites by design (points to the latest spawn). This matches typical action combat usage:
 *   one spawn window -> zero or more manual-hit events -> destroy.
 */
USTRUCT()
struct FBulletInstanceRegistry
{
	GENERATED_BODY()

	/** Alias -> InstanceId. Overwrite when alias already exists. */
	UPROPERTY(Transient)
	TMap<FName, int32> InstanceAliasMap;

	void ClearAliasMap() { InstanceAliasMap.Reset(); }
	bool RemoveAlias(FName Alias) { return !Alias.IsNone() && InstanceAliasMap.Remove(Alias) > 0; }
	void AddToAliasMap(FName Alias, int32 InstanceId)
	{
		if (Alias.IsNone())
		{
			return;
		}
		if (InstanceId == INDEX_NONE)
		{
			InstanceAliasMap.Remove(Alias);
			return;
		}
		InstanceAliasMap.Add(Alias, InstanceId);
	}
	
	int32 GetInstanceAlias(FName Alias) const
	{
		if (Alias.IsNone())
		{
			return INDEX_NONE;
		}
		if (const int32* Found = InstanceAliasMap.Find(Alias))
		{
			return *Found;
		}
		return INDEX_NONE;
	}
};

UCLASS(ClassGroup = (BulletSystem), meta = (BlueprintSpawnableComponent))
class BULLETGAME_API UBulletSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBulletSystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Inject config from game-side (e.g. HeroComponent reads PawnData and forwards BulletConfig here).
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	void SetBulletConfig(UBulletConfig* InConfig);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem")
	UBulletConfig* GetBulletConfig() const { return BulletConfig; }

	/** Resolve a runtime bullet instance by alias. Invalid entries are pruned. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|InstanceRegistry")
	int32 GetInstanceIdByAlias(FName InstanceAlias) const;

	/** Set or overwrite alias -> InstanceId. Passing INDEX_NONE removes the alias. */
	UFUNCTION(BlueprintCallable, Category = "BulletSystem|InstanceRegistry")
	void SetInstanceIdAlias(FName InstanceAlias, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "BulletSystem|InstanceRegistry")
	bool RemoveInstanceAlias(FName InstanceAlias);

	UFUNCTION(BlueprintCallable, Category = "BulletSystem|InstanceRegistry")
	void ClearInstanceAliases();

	/** True if alias resolves to a live bullet instance. Invalid entries are pruned. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|InstanceRegistry")
	bool IsInstanceAliasValid(FName InstanceAlias) const;

	// Local spawn entry.
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	int32 SpawnBullet(FName BulletId, const FBulletInitParams& InitParams);

	// Request destroy for a bullet instance (local).
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	bool DestroyBullet(int32 InstanceId, EBulletDestroyReason Reason, bool bSpawnChildren);

	// Manual-hit trigger: process stored overlaps as hits (fires OnHit logic chain, interact, collision response).
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	int32 ProcessManualHits(int32 InstanceId, bool bResetHitActorsBefore, bool bApplyCollisionResponse);

private:
	int32 SpawnBulletInternal(FName BulletId, const FBulletInitParams& InitParams);
	bool DestroyBulletInternal(int32 InstanceId, EBulletDestroyReason Reason, bool bSpawnChildren) const;
	int32 ProcessManualHitsInternal(int32 InstanceId, bool bResetHitActorsBefore, bool bApplyCollisionResponse) const;
	bool IsInstanceIdValid(int32 InstanceId) const;

	UPROPERTY()
	TObjectPtr<UBulletConfig> BulletConfig;

	UPROPERTY(Transient)
	mutable FBulletInstanceRegistry InstanceRegistry;
};
