#pragma once

// BulletSystem: BulletSystemComponent.h
// Entry-point glue for gameplay code/blueprints. Bullet simulation remains world-scoped
// (UBulletWorldSubsystem/UBulletController).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BulletSystemTypes.h"

#include "BulletSystemComponent.generated.h"

class UBulletConfig;

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

	// Local spawn entry.
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	int32 SpawnBullet(FName BulletID, const FBulletInitParams& InitParams);

	// Request destroy for a bullet instance (local).
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	bool DestroyBullet(int32 InstanceId, EBulletDestroyReason Reason, bool bSpawnChildren);

	// Manual-hit trigger: process stored overlaps as hits (fires OnHit logic chain, interact, collision response).
	UFUNCTION(BlueprintCallable, Category = "BulletSystem")
	int32 ProcessManualHits(int32 InstanceId, bool bResetHitActorsBefore, bool bApplyCollisionResponse);

private:
	int32 SpawnBulletInternal(FName BulletID, const FBulletInitParams& InitParams);
	bool DestroyBulletInternal(int32 InstanceId, EBulletDestroyReason Reason, bool bSpawnChildren) const;
	int32 ProcessManualHitsInternal(int32 InstanceId, bool bResetHitActorsBefore, bool bApplyCollisionResponse) const;

	UPROPERTY()
	TObjectPtr<UBulletConfig> BulletConfig;
};
