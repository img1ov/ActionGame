// BulletSystem: BulletSystemNotifies.cpp

#include "Notifies/BulletSystemNotifies.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "BulletLogChannels.h"
#include "BulletSystemComponent.h"
#include "BulletSystemInterface.h"
#include "Controller/BulletController.h"
#include "Controller/BulletWorldSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAN_SpawnBullet::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	OnNotify(MeshComp, Animation, EventReference);
}

void UAN_SpawnBullet::OnNotify_Implementation(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const
{
	(void)Animation;
	(void)EventReference;

	if (BulletId.IsNone())
	{
		return;
	}

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor)
	{
		return;
	}

	UBulletSystemComponent* BulletComp = nullptr;
	if (OwnerActor->GetClass()->ImplementsInterface(UBulletSystemInterface::StaticClass()))
	{
		BulletComp = IBulletSystemInterface::Execute_GetBulletSystemComponent(OwnerActor);
	}
	if (!BulletComp)
	{
		BulletComp = OwnerActor->FindComponentByClass<UBulletSystemComponent>();
	}
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	// If no explicit alias is provided, fall back to BulletId. This keeps simple cases working
	// (single bullet per ability window) while still allowing explicit naming for complex flows.
	const FName Key = InstanceKey.IsNone() ? BulletId : InstanceKey;

	FBulletInitParams InitParams;
	InitParams.Owner = OwnerActor;
	if (MeshComp && !SpawnSocketName.IsNone() && MeshComp->DoesSocketExist(SpawnSocketName))
	{
		InitParams.SpawnTransform = MeshComp->GetSocketTransform(SpawnSocketName, RTS_World);
	}
	else
	{
		InitParams.SpawnTransform = MeshComp ? MeshComp->GetComponentTransform() : OwnerActor->GetActorTransform();
	}
	InitParams.InstanceKey = Key;
	InitParams.Payload.HitReactImpulse = HitReactImpulse;

	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		const int32 InstanceId = BulletComp->SpawnBullet(BulletId, InitParams);
		if (bForceCollisionOnSimProxy)
		{
			if (UWorld* World = OwnerActor->GetWorld())
			{
				if (UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>())
				{
					if (UBulletController* Controller = Subsystem->GetController())
					{
						(void)Controller->SetCollisionEnabled(InstanceId, /*bEnabled*/ true, /*bClearOverlaps*/ false, /*bResetHitActors*/ false);
					}
				}
			}
		}
		return;
	}

	if (SpawnEventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->BulletId = BulletId;
		OptionalObj->InitParams = InitParams;
		OptionalObj->InstanceKey = Key;

		FGameplayEventData EventData;
		EventData.EventTag = SpawnEventTag;
		EventData.Instigator = OwnerActor;
		EventData.Target = OwnerActor;
		EventData.OptionalObject = OptionalObj;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, SpawnEventTag, EventData);
		return;
	}

	// No GA tag: spawn locally (usable for non-GAS gameplay).
	(void)BulletComp->SpawnBullet(BulletId, InitParams);
}

void UAN_ProcessManualHits::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	OnNotify(MeshComp, Animation, EventReference);
}

void UAN_ProcessManualHits::OnNotify_Implementation(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const
{
	(void)Animation;
	(void)EventReference;

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor)
	{
		return;
	}

	UBulletSystemComponent* BulletComp = nullptr;
	if (OwnerActor->GetClass()->ImplementsInterface(UBulletSystemInterface::StaticClass()))
	{
		BulletComp = IBulletSystemInterface::Execute_GetBulletSystemComponent(OwnerActor);
	}
	if (!BulletComp)
	{
		BulletComp = OwnerActor->FindComponentByClass<UBulletSystemComponent>();
	}
	if (!BulletComp)
	{
		return;
	}

	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		if (InstanceKey.IsNone())
		{
			UE_LOG(LogBullet, Warning, TEXT("Bullet ProcessManualHits notify skipped: InstanceKey missing. Owner=%s"),
				*GetNameSafe(OwnerActor));
			return;
		}

		const int32 InstanceId = BulletComp->GetInstanceIdByKey(InstanceKey);
		if (InstanceId != INDEX_NONE)
		{
			(void)BulletComp->ProcessManualHits(InstanceId, bResetHitActorsBefore, /*bApplyCollisionResponse*/ true);
		}
		return;
	}

	if (EventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->InstanceKey = InstanceKey;
		OptionalObj->bResetHitActorsBefore = bResetHitActorsBefore;

		FGameplayEventData EventData;
		EventData.EventTag = EventTag;
		EventData.Instigator = OwnerActor;
		EventData.Target = OwnerActor;
		EventData.OptionalObject = OptionalObj;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, EventData);
		return;
	}

	// No GA tag: process locally.
	if (InstanceKey.IsNone())
	{
		UE_LOG(LogBullet, Warning, TEXT("Bullet ProcessManualHits notify skipped: InstanceKey missing (no GA tag). Owner=%s"),
			*GetNameSafe(OwnerActor));
		return;
	}

	const int32 InstanceId = BulletComp->GetInstanceIdByKey(InstanceKey);
	if (InstanceId != INDEX_NONE)
	{
		(void)BulletComp->ProcessManualHits(InstanceId, bResetHitActorsBefore, /*bApplyCollisionResponse*/ true);
	}
}

void UAN_DestroyBullet::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	OnNotify(MeshComp, Animation, EventReference);
}

void UAN_DestroyBullet::OnNotify_Implementation(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const
{
	(void)Animation;
	(void)EventReference;

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor)
	{
		return;
	}

	UBulletSystemComponent* BulletComp = nullptr;
	if (OwnerActor->GetClass()->ImplementsInterface(UBulletSystemInterface::StaticClass()))
	{
		BulletComp = IBulletSystemInterface::Execute_GetBulletSystemComponent(OwnerActor);
	}
	if (!BulletComp)
	{
		BulletComp = OwnerActor->FindComponentByClass<UBulletSystemComponent>();
	}
	if (!BulletComp)
	{
		return;
	}

	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		if (InstanceKey.IsNone())
		{
			UE_LOG(LogBullet, Warning, TEXT("Bullet Destroy notify skipped: InstanceKey missing. Owner=%s"),
				*GetNameSafe(OwnerActor));
			return;
		}

		const int32 InstanceId = BulletComp->GetInstanceIdByKey(InstanceKey);
		if (InstanceId != INDEX_NONE)
		{
			(void)BulletComp->DestroyBullet(InstanceId);
			BulletComp->RemoveInstanceKey(InstanceKey);
		}
		return;
	}

	if (EventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->InstanceKey = InstanceKey;

		FGameplayEventData EventData;
		EventData.EventTag = EventTag;
		EventData.Instigator = OwnerActor;
		EventData.Target = OwnerActor;
		EventData.OptionalObject = OptionalObj;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, EventData);
		return;
	}

	// No GA tag: destroy locally.
	if (InstanceKey.IsNone())
	{
		UE_LOG(LogBullet, Warning, TEXT("Bullet Destroy notify skipped: InstanceKey missing (no GA tag). Owner=%s"),
			*GetNameSafe(OwnerActor));
		return;
	}

	const int32 InstanceId = BulletComp->GetInstanceIdByKey(InstanceKey);
	if (InstanceId != INDEX_NONE)
	{
		(void)BulletComp->DestroyBullet(InstanceId);
		BulletComp->RemoveInstanceKey(InstanceKey);
	}
}

void UANS_SpawnBullet::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	OnBegin(MeshComp, Animation, TotalDuration, EventReference);
}

void UANS_SpawnBullet::OnBegin_Implementation(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) const
{
	(void)Animation;
	(void)TotalDuration;
	(void)EventReference;

	if (BulletId.IsNone())
	{
		return;
	}

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor)
	{
		return;
	}

	UBulletSystemComponent* BulletComp = nullptr;
	if (OwnerActor->GetClass()->ImplementsInterface(UBulletSystemInterface::StaticClass()))
	{
		BulletComp = IBulletSystemInterface::Execute_GetBulletSystemComponent(OwnerActor);
	}
	if (!BulletComp)
	{
		BulletComp = OwnerActor->FindComponentByClass<UBulletSystemComponent>();
	}
	if (!BulletComp)
	{
		return;
	}

	const FName Key = InstanceKey.IsNone() ? BulletId : InstanceKey;

	FBulletInitParams InitParams;
	InitParams.Owner = OwnerActor;
	if (MeshComp && !SpawnSocketName.IsNone() && MeshComp->DoesSocketExist(SpawnSocketName))
	{
		InitParams.SpawnTransform = MeshComp->GetSocketTransform(SpawnSocketName, RTS_World);
	}
	else
	{
		InitParams.SpawnTransform = MeshComp ? MeshComp->GetComponentTransform() : OwnerActor->GetActorTransform();
	}
	InitParams.InstanceKey = Key;
	InitParams.Payload.HitReactImpulse = HitReactImpulse;

	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		const int32 InstanceId = BulletComp->SpawnBullet(BulletId, InitParams);
		if (bForceCollisionOnSimProxy)
		{
			if (UWorld* World = OwnerActor->GetWorld())
			{
				if (UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>())
				{
					if (UBulletController* Controller = Subsystem->GetController())
					{
						(void)Controller->SetCollisionEnabled(InstanceId, /*bEnabled*/ true, /*bClearOverlaps*/ false, /*bResetHitActors*/ false);
					}
				}
			}
		}
		return;
	}

	if (SpawnEventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->BulletId = BulletId;
		OptionalObj->InitParams = InitParams;
		OptionalObj->InstanceKey = Key;

		FGameplayEventData EventData;
		EventData.EventTag = SpawnEventTag;
		EventData.Instigator = OwnerActor;
		EventData.Target = OwnerActor;
		EventData.OptionalObject = OptionalObj;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, SpawnEventTag, EventData);
		return;
	}

	(void)BulletComp->SpawnBullet(BulletId, InitParams);
}

void UANS_SpawnBullet::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	OnEnd(MeshComp, Animation, EventReference);
}

void UANS_SpawnBullet::OnEnd_Implementation(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const
{
	(void)Animation;
	(void)EventReference;

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor)
	{
		return;
	}

	UBulletSystemComponent* BulletComp = nullptr;
	if (OwnerActor->GetClass()->ImplementsInterface(UBulletSystemInterface::StaticClass()))
	{
		BulletComp = IBulletSystemInterface::Execute_GetBulletSystemComponent(OwnerActor);
	}
	if (!BulletComp)
	{
		BulletComp = OwnerActor->FindComponentByClass<UBulletSystemComponent>();
	}
	if (!BulletComp)
	{
		return;
	}

	const FName Key = InstanceKey.IsNone() ? BulletId : InstanceKey;

	if (OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		const int32 InstanceId = BulletComp->GetInstanceIdByKey(Key);
		if (InstanceId != INDEX_NONE)
		{
			(void)BulletComp->DestroyBullet(InstanceId);
			BulletComp->RemoveInstanceKey(Key);
		}
		return;
	}

	if (EndEventTag.IsValid())
	{
		UBulletNotifyOptionalObject* OptionalObj = NewObject<UBulletNotifyOptionalObject>(OwnerActor);
		OptionalObj->InstanceKey = Key;

		FGameplayEventData EventData;
		EventData.EventTag = EndEventTag;
		EventData.Instigator = OwnerActor;
		EventData.Target = OwnerActor;
		EventData.OptionalObject = OptionalObj;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EndEventTag, EventData);
		return;
	}

	const int32 InstanceId = BulletComp->GetInstanceIdByKey(Key);
	if (InstanceId != INDEX_NONE)
	{
		(void)BulletComp->DestroyBullet(InstanceId);
		BulletComp->RemoveInstanceKey(Key);
	}
}
