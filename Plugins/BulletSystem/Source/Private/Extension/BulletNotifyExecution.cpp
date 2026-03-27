#include "Extension/BulletNotifyExecution.h"

#include "Extension/BulletSystemNotifies.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "BulletLogChannels.h"
#include "BulletSystemComponent.h"
#include "BulletSystemInterface.h"
#include "Controller/BulletController.h"
#include "Controller/BulletWorldSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace BulletNotifyExecution
{
UBulletSystemComponent* ResolveBulletComponent(AActor* OwnerActor)
{
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (OwnerActor->GetClass()->ImplementsInterface(UBulletSystemInterface::StaticClass()))
	{
		if (UBulletSystemComponent* BulletComp = IBulletSystemInterface::Execute_GetBulletSystemComponent(OwnerActor))
		{
			return BulletComp;
		}
	}

	return OwnerActor->FindComponentByClass<UBulletSystemComponent>();
}

FName ResolveInstanceKey(const FName BulletId, const FName InstanceKey)
{
	return InstanceKey.IsNone() ? BulletId : InstanceKey;
}

bool TryBuildSpawnInitParams(AActor* OwnerActor, USkeletalMeshComponent* MeshComp, const FName SpawnSocketName, const FName ResolvedInstanceKey, const FHitReactImpulse& HitReactImpulse, FBulletInitParams& OutInitParams)
{
	if (!OwnerActor)
	{
		return false;
	}

	OutInitParams = FBulletInitParams();
	OutInitParams.Owner = OwnerActor;
	if (MeshComp && !SpawnSocketName.IsNone() && MeshComp->DoesSocketExist(SpawnSocketName))
	{
		OutInitParams.SpawnTransform = MeshComp->GetSocketTransform(SpawnSocketName, RTS_World);
	}
	else
	{
		OutInitParams.SpawnTransform = MeshComp ? MeshComp->GetComponentTransform() : OwnerActor->GetActorTransform();
	}
	OutInitParams.InstanceKey = ResolvedInstanceKey;
	OutInitParams.Payload.HitReactImpulse = HitReactImpulse;
	return true;
}

void SendGameplayEvent(AActor* OwnerActor, const FGameplayTag& EventTag, UBulletNotifyOptionalObject* OptionalObject)
{
	if (!OwnerActor || !EventTag.IsValid())
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = OwnerActor;
	EventData.Target = OwnerActor;
	EventData.OptionalObject = OptionalObject;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, EventData);
}

static void EnableCollisionForSimProxyVisuals(AActor* OwnerActor, const int32 InstanceId, const bool bForceCollisionOnSimProxy)
{
	if (!bForceCollisionOnSimProxy || !OwnerActor || InstanceId == INDEX_NONE)
	{
		return;
	}

	if (UWorld* World = OwnerActor->GetWorld())
	{
		if (UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>())
		{
			if (UBulletController* Controller = Subsystem->GetController())
			{
				(void)Controller->SetCollisionEnabled(InstanceId, true, false);
			}
		}
	}
}

void HandleSpawnNotify(const UAN_SpawnBullet& Notify, USkeletalMeshComponent* MeshComp)
{
	if (Notify.BulletId.IsNone())
	{
		return;
	}

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = ResolveBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	const FName Key = ResolveInstanceKey(Notify.BulletId, Notify.InstanceKey);
	FBulletInitParams InitParams;
	if (!TryBuildSpawnInitParams(OwnerActor, MeshComp, Notify.SpawnSocketName, Key, Notify.HitReactImpulse, InitParams))
	{
		return;
	}

	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		const int32 InstanceId = BulletComp->SpawnBullet(Notify.BulletId, InitParams);
		EnableCollisionForSimProxyVisuals(OwnerActor, InstanceId, Notify.bForceCollisionOnSimProxy);
		return;
	}

	if (Notify.SpawnEventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->BulletId = Notify.BulletId;
		OptionalObj->InitParams = InitParams;
		OptionalObj->InstanceKey = Key;
		SendGameplayEvent(OwnerActor, Notify.SpawnEventTag, OptionalObj);
		return;
	}

	(void)BulletComp->SpawnBullet(Notify.BulletId, InitParams);
}

void HandleProcessManualHitsNotify(const UAN_ProcessManualHits& Notify, USkeletalMeshComponent* MeshComp)
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = ResolveBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		if (Notify.InstanceKey.IsNone())
		{
			UE_LOG(LogBullet, Warning, TEXT("Bullet ProcessManualHits notify skipped: InstanceKey missing. Owner=%s"), *GetNameSafe(OwnerActor));
			return;
		}

		const int32 InstanceId = BulletComp->GetInstanceIdByKey(Notify.InstanceKey);
		if (InstanceId != INDEX_NONE)
		{
			(void)BulletComp->ProcessManualHits(InstanceId, true);
		}
		return;
	}

	if (Notify.EventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->InstanceKey = Notify.InstanceKey;
		SendGameplayEvent(OwnerActor, Notify.EventTag, OptionalObj);
		return;
	}

	if (Notify.InstanceKey.IsNone())
	{
		UE_LOG(LogBullet, Warning, TEXT("Bullet ProcessManualHits notify skipped: InstanceKey missing (no GA tag). Owner=%s"), *GetNameSafe(OwnerActor));
		return;
	}

	const int32 InstanceId = BulletComp->GetInstanceIdByKey(Notify.InstanceKey);
	if (InstanceId != INDEX_NONE)
	{
		(void)BulletComp->ProcessManualHits(InstanceId, true);
	}
}

void HandleDestroyNotify(const UAN_DestroyBullet& Notify, USkeletalMeshComponent* MeshComp)
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = ResolveBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		if (Notify.InstanceKey.IsNone())
		{
			UE_LOG(LogBullet, Warning, TEXT("Bullet Destroy notify skipped: InstanceKey missing. Owner=%s"), *GetNameSafe(OwnerActor));
			return;
		}

		const int32 InstanceId = BulletComp->ConsumeOldestInstanceIdByKey(Notify.InstanceKey);
		if (InstanceId != INDEX_NONE)
		{
			(void)BulletComp->DestroyBullet(InstanceId);
		}
		return;
	}

	if (Notify.EventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->InstanceKey = Notify.InstanceKey;
		SendGameplayEvent(OwnerActor, Notify.EventTag, OptionalObj);
		return;
	}

	if (Notify.InstanceKey.IsNone())
	{
		UE_LOG(LogBullet, Warning, TEXT("Bullet Destroy notify skipped: InstanceKey missing (no GA tag). Owner=%s"), *GetNameSafe(OwnerActor));
		return;
	}

	const int32 InstanceId = BulletComp->ConsumeOldestInstanceIdByKey(Notify.InstanceKey);
	if (InstanceId != INDEX_NONE)
	{
		(void)BulletComp->DestroyBullet(InstanceId);
	}
}

void HandleSpawnStateBegin(const UANS_SpawnBullet& Notify, USkeletalMeshComponent* MeshComp)
{
	if (Notify.BulletId.IsNone())
	{
		return;
	}

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = ResolveBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	const FName Key = ResolveInstanceKey(Notify.BulletId, Notify.InstanceKey);
	FBulletInitParams InitParams;
	if (!TryBuildSpawnInitParams(OwnerActor, MeshComp, Notify.SpawnSocketName, Key, Notify.HitReactImpulse, InitParams))
	{
		return;
	}

	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		const int32 InstanceId = BulletComp->SpawnBullet(Notify.BulletId, InitParams);
		EnableCollisionForSimProxyVisuals(OwnerActor, InstanceId, Notify.bForceCollisionOnSimProxy);
		return;
	}

	if (Notify.SpawnEventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->BulletId = Notify.BulletId;
		OptionalObj->InitParams = InitParams;
		OptionalObj->InstanceKey = Key;
		SendGameplayEvent(OwnerActor, Notify.SpawnEventTag, OptionalObj);
		return;
	}

	(void)BulletComp->SpawnBullet(Notify.BulletId, InitParams);
}

void HandleSpawnStateEnd(const UANS_SpawnBullet& Notify, USkeletalMeshComponent* MeshComp)
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = ResolveBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	const FName Key = ResolveInstanceKey(Notify.BulletId, Notify.InstanceKey);
	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		const int32 InstanceId = BulletComp->ConsumeOldestInstanceIdByKey(Key);
		if (InstanceId != INDEX_NONE)
		{
			(void)BulletComp->DestroyBullet(InstanceId);
		}
		return;
	}

	if (Notify.EndEventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->InstanceKey = Key;
		SendGameplayEvent(OwnerActor, Notify.EndEventTag, OptionalObj);
		return;
	}

	const int32 InstanceId = BulletComp->ConsumeOldestInstanceIdByKey(Key);
	if (InstanceId != INDEX_NONE)
	{
		(void)BulletComp->DestroyBullet(InstanceId);
	}
}
}
