#pragma once

#include "CoreMinimal.h"
#include "BulletSystemTypes.h"

class AActor;
class UBulletSystemComponent;
class UBulletNotifyOptionalObject;
class UAN_SpawnBullet;
class UAN_ProcessManualHits;
class UAN_DestroyBullet;
class UANS_SpawnBullet;
class USkeletalMeshComponent;
struct FGameplayEventData;
struct FAnimNotifyEventReference;

namespace BulletNotifyExecution
{
	UBulletSystemComponent* ResolveBulletComponent(AActor* OwnerActor);
	FName ResolveInstanceKey(FName BulletId, FName InstanceKey);
	bool TryBuildSpawnInitParams(AActor* OwnerActor, USkeletalMeshComponent* MeshComp, FName SpawnSocketName, FName ResolvedInstanceKey, const FHitReactImpulse& HitReactImpulse, FBulletInitParams& OutInitParams);
	void SendGameplayEvent(AActor* OwnerActor, const FGameplayTag& EventTag, UBulletNotifyOptionalObject* OptionalObject);
	void HandleSpawnNotify(const UAN_SpawnBullet& Notify, USkeletalMeshComponent* MeshComp);
	void HandleProcessManualHitsNotify(const UAN_ProcessManualHits& Notify, USkeletalMeshComponent* MeshComp);
	void HandleDestroyNotify(const UAN_DestroyBullet& Notify, USkeletalMeshComponent* MeshComp);
	void HandleSpawnStateBegin(const UANS_SpawnBullet& Notify, USkeletalMeshComponent* MeshComp);
	void HandleSpawnStateEnd(const UANS_SpawnBullet& Notify, USkeletalMeshComponent* MeshComp);
}
