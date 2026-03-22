// BulletSystem: BulletSystemNotifies.cpp

#include "Notifies/BulletSystemNotifies.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "BulletLogChannels.h"
#include "BulletSystemComponent.h"
#include "BulletSystemInterface.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/Guid.h"

namespace
{
	UBulletSystemComponent* FindBulletComponent(AActor* OwnerActor)
	{
		if (!OwnerActor)
		{
			return nullptr;
		}

		if (OwnerActor->GetClass()->ImplementsInterface(UBulletSystemInterface::StaticClass()))
		{
			if (UBulletSystemComponent* Comp = IBulletSystemInterface::Execute_GetBulletSystemComponent(OwnerActor))
			{
				return Comp;
			}
		}

		return OwnerActor->FindComponentByClass<UBulletSystemComponent>();
	}

	FTransform BuildSpawnTransform(USkeletalMeshComponent* MeshComp, FName SpawnSocketName)
	{
		if (!MeshComp)
		{
			return FTransform::Identity;
		}

		if (!SpawnSocketName.IsNone() && MeshComp->DoesSocketExist(SpawnSocketName))
		{
			return MeshComp->GetSocketTransform(SpawnSocketName, RTS_World);
		}

		return MeshComp->GetComponentTransform();
	}

	void SendBulletEvent(AActor* OwnerActor, FGameplayTag EventTag, const UBulletSystemNotifyPayload* PayloadObj)
	{
		if (!OwnerActor || !EventTag.IsValid())
		{
			return;
		}

		FGameplayEventData EventData;
		EventData.EventTag = EventTag;
		EventData.Instigator = OwnerActor;
		EventData.Target = OwnerActor;
		EventData.OptionalObject = const_cast<UBulletSystemNotifyPayload*>(PayloadObj);

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, EventData);
	}

	bool IsSimulatedProxy(const AActor* OwnerActor)
	{
		return OwnerActor && OwnerActor->GetLocalRole() == ROLE_SimulatedProxy;
	}
}

void UAN_SpawnBullet::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (BulletId.IsNone())
	{
		return;
	}

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = FindBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	// If no explicit alias is provided, fall back to BulletId. This keeps simple cases working
	// (single bullet per ability window) while still allowing explicit naming for complex flows.
	const FName Alias = InstanceAlias.IsNone() ? BulletId : InstanceAlias;

	if (IsSimulatedProxy(OwnerActor))
	{
		FBulletInitParams InitParams;
		InitParams.Owner = OwnerActor;
		InitParams.SpawnTransform = BuildSpawnTransform(MeshComp, SpawnSocketName);
		InitParams.InstanceAlias = Alias;
		if (bForceCollisionOnSimProxy)
		{
			InitParams.CollisionEnabledOverride = 1;
		}
		(void)BulletComp->SpawnBullet(BulletId, InitParams);
		return;
	}

	if (SpawnEventTag.IsValid())
	{
		UBulletSystemNotifyPayload* PayloadObj = NewObject<UBulletSystemNotifyPayload>(OwnerActor);
		PayloadObj->BulletId = BulletId;
		PayloadObj->InstanceAlias = Alias;
		SendBulletEvent(OwnerActor, SpawnEventTag, PayloadObj);
		return;
	}

	// No GA tag: spawn locally (usable for non-GAS gameplay).
	FBulletInitParams InitParams;
	InitParams.Owner = OwnerActor;
	InitParams.SpawnTransform = BuildSpawnTransform(MeshComp, SpawnSocketName);
	InitParams.InstanceAlias = Alias;
	(void)BulletComp->SpawnBullet(BulletId, InitParams);
}

void UAN_ProcessManualHits::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = FindBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	if (IsSimulatedProxy(OwnerActor))
	{
		if (InstanceAlias.IsNone())
		{
			UE_LOG(LogBullet, Warning, TEXT("Bullet ProcessManualHits notify skipped: InstanceAlias missing. Owner=%s"),
				*GetNameSafe(OwnerActor));
			return;
		}

		const int32 InstanceId = BulletComp->GetInstanceIdByAlias(InstanceAlias);
		if (InstanceId != INDEX_NONE)
		{
			(void)BulletComp->ProcessManualHits(InstanceId, bResetHitActorsBefore, bApplyCollisionResponse);
		}
		return;
	}

	if (EventTag.IsValid())
	{
		UBulletSystemNotifyPayload* PayloadObj = NewObject<UBulletSystemNotifyPayload>(OwnerActor);
		PayloadObj->InstanceAlias = InstanceAlias;
		PayloadObj->bResetHitActorsBefore = bResetHitActorsBefore;
		PayloadObj->bApplyCollisionResponse = bApplyCollisionResponse;
		SendBulletEvent(OwnerActor, EventTag, PayloadObj);
		return;
	}

	// No GA tag: process locally.
	if (InstanceAlias.IsNone())
	{
		UE_LOG(LogBullet, Warning, TEXT("Bullet ProcessManualHits notify skipped: InstanceAlias missing (no GA tag). Owner=%s"),
			*GetNameSafe(OwnerActor));
		return;
	}

	const int32 InstanceId = BulletComp->GetInstanceIdByAlias(InstanceAlias);
	if (InstanceId != INDEX_NONE)
	{
		(void)BulletComp->ProcessManualHits(InstanceId, bResetHitActorsBefore, bApplyCollisionResponse);
	}
}

void UAN_DestroyBullet::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = FindBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	if (IsSimulatedProxy(OwnerActor))
	{
		if (InstanceAlias.IsNone())
		{
			UE_LOG(LogBullet, Warning, TEXT("Bullet Destroy notify skipped: InstanceAlias missing. Owner=%s"),
				*GetNameSafe(OwnerActor));
			return;
		}

		const int32 InstanceId = BulletComp->GetInstanceIdByAlias(InstanceAlias);
		if (InstanceId != INDEX_NONE)
		{
			(void)BulletComp->DestroyBullet(InstanceId, Reason, bSpawnChildren);
			BulletComp->RemoveInstanceAlias(InstanceAlias);
		}
		return;
	}

	if (EventTag.IsValid())
	{
		UBulletSystemNotifyPayload* PayloadObj = NewObject<UBulletSystemNotifyPayload>(OwnerActor);
		PayloadObj->InstanceAlias = InstanceAlias;
		PayloadObj->DestroyReason = Reason;
		PayloadObj->bSpawnChildren = bSpawnChildren;
		SendBulletEvent(OwnerActor, EventTag, PayloadObj);
		return;
	}

	// No GA tag: destroy locally.
	if (InstanceAlias.IsNone())
	{
		UE_LOG(LogBullet, Warning, TEXT("Bullet Destroy notify skipped: InstanceAlias missing (no GA tag). Owner=%s"),
			*GetNameSafe(OwnerActor));
		return;
	}

	const int32 InstanceId = BulletComp->GetInstanceIdByAlias(InstanceAlias);
	if (InstanceId != INDEX_NONE)
	{
		(void)BulletComp->DestroyBullet(InstanceId, Reason, bSpawnChildren);
		BulletComp->RemoveInstanceAlias(InstanceAlias);
	}
}

void UANS_SpawnBullet::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (NotifyId.IsNone())
	{
		NotifyId = FName(*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
	}
}

void UANS_SpawnBullet::PostLoad()
{
	Super::PostLoad();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (NotifyId.IsNone())
	{
		NotifyId = FName(*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
	}
}

void UANS_SpawnBullet::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	NotifyId = FName(*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
}

FName UANS_SpawnBullet::GetEffectiveAlias() const
{
	return InstanceAlias.IsNone() ? NotifyId : InstanceAlias;
}

void UANS_SpawnBullet::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (BulletId.IsNone())
	{
		return;
	}

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = FindBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	const FName Alias = GetEffectiveAlias();

	if (IsSimulatedProxy(OwnerActor))
	{
		FBulletInitParams InitParams;
		InitParams.Owner = OwnerActor;
		InitParams.SpawnTransform = BuildSpawnTransform(MeshComp, SpawnSocketName);
		InitParams.InstanceAlias = Alias;
		if (bForceCollisionOnSimProxy)
		{
			InitParams.CollisionEnabledOverride = 1;
		}
		(void)BulletComp->SpawnBullet(BulletId, InitParams);
		return;
	}

	if (SpawnEventTag.IsValid())
	{
		UBulletSystemNotifyPayload* PayloadObj = NewObject<UBulletSystemNotifyPayload>(OwnerActor);
		PayloadObj->BulletId = BulletId;
		PayloadObj->InstanceAlias = Alias;
		SendBulletEvent(OwnerActor, SpawnEventTag, PayloadObj);
		return;
	}

	FBulletInitParams InitParams;
	InitParams.Owner = OwnerActor;
	InitParams.SpawnTransform = BuildSpawnTransform(MeshComp, SpawnSocketName);
	InitParams.InstanceAlias = Alias;
	(void)BulletComp->SpawnBullet(BulletId, InitParams);
}

void UANS_SpawnBullet::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UBulletSystemComponent* BulletComp = FindBulletComponent(OwnerActor);
	if (!OwnerActor || !BulletComp)
	{
		return;
	}

	const FName Alias = GetEffectiveAlias();

	if (IsSimulatedProxy(OwnerActor))
	{
		const int32 InstanceId = BulletComp->GetInstanceIdByAlias(Alias);
		if (InstanceId != INDEX_NONE)
		{
			(void)BulletComp->DestroyBullet(InstanceId, EndReason, bSpawnChildrenOnEnd);
			BulletComp->RemoveInstanceAlias(Alias);
		}
		return;
	}

	if (EndEventTag.IsValid())
	{
		UBulletSystemNotifyPayload* PayloadObj = NewObject<UBulletSystemNotifyPayload>(OwnerActor);
		PayloadObj->InstanceAlias = Alias;
		PayloadObj->DestroyReason = EndReason;
		PayloadObj->bSpawnChildren = bSpawnChildrenOnEnd;
		SendBulletEvent(OwnerActor, EndEventTag, PayloadObj);
		return;
	}

	const int32 InstanceId = BulletComp->GetInstanceIdByAlias(Alias);
	if (InstanceId != INDEX_NONE)
	{
		(void)BulletComp->DestroyBullet(InstanceId, EndReason, bSpawnChildrenOnEnd);
		BulletComp->RemoveInstanceAlias(Alias);
	}
}
