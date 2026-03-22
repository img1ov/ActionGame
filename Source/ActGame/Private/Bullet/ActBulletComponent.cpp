// Copyright

#include "Bullet/ActBulletComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "ActLogChannels.h"
#include "Bullet/Notifies/ActBulletAnimNotifyPayload.h"
#include "Component/BulletSystemComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UActBulletComponent::UActBulletComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

UBulletSystemComponent* UActBulletComponent::FindBulletSystemComponent() const
{
	AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->FindComponentByClass<UBulletSystemComponent>() : nullptr;
}

bool UActBulletComponent::IsSimulatedProxy() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->GetLocalRole() == ROLE_SimulatedProxy;
}

bool UActBulletComponent::ShouldRouteToGA() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	const ENetRole Role = OwnerActor->GetLocalRole();
	return Role == ROLE_Authority || Role == ROLE_AutonomousProxy;
}

FTransform UActBulletComponent::BuildSpawnTransform(USkeletalMeshComponent* MeshComp, FName SpawnSocketName) const
{
	if (MeshComp)
	{
		if (!SpawnSocketName.IsNone() && MeshComp->DoesSocketExist(SpawnSocketName))
		{
			return MeshComp->GetSocketTransform(SpawnSocketName, RTS_World);
		}
		return MeshComp->GetComponentTransform();
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetActorTransform();
	}

	return FTransform::Identity;
}

void UActBulletComponent::SendBulletGameplayEvent(
	FGameplayTag EventTag,
	int32 SlotIndex,
	FName BulletID,
	FName NotifyId,
	bool bResetHitActorsBefore,
	bool bApplyCollisionResponse,
	EBulletDestroyReason DestroyReason,
	bool bSpawnChildren) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !EventTag.IsValid())
	{
		return;
	}

	UActBulletAnimNotifyPayload* PayloadObj = NewObject<UActBulletAnimNotifyPayload>(OwnerActor);
	PayloadObj->SlotIndex = SlotIndex;
	PayloadObj->BulletID = BulletID;
	PayloadObj->NotifyId = NotifyId;
	PayloadObj->bResetHitActorsBefore = bResetHitActorsBefore;
	PayloadObj->bApplyCollisionResponse = bApplyCollisionResponse;
	PayloadObj->DestroyReason = DestroyReason;
	PayloadObj->bSpawnChildren = bSpawnChildren;

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = OwnerActor;
	EventData.Target = OwnerActor;
	EventData.OptionalObject = PayloadObj;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, EventData);
}

void UActBulletComponent::Notify_SpawnBullet(USkeletalMeshComponent* MeshComp, FName BulletID, int32 SlotIndex, FName SpawnSocketName, FGameplayTag SpawnEventTag, bool bForceCollisionOnSimProxy)
{
	if (BulletID.IsNone())
	{
		return;
	}

	// Simulated proxies do not execute GA. They spawn bullets locally for visuals.
	if (IsSimulatedProxy())
	{
		UBulletSystemComponent* BulletSystem = FindBulletSystemComponent();
		if (!BulletSystem)
		{
			UE_LOG(LogAct, Warning, TEXT("ActBulletComponent: SimProxy SpawnBullet failed (BulletSystemComponent missing). Owner=%s BulletID=%s"),
				*GetNameSafe(GetOwner()),
				*BulletID.ToString());
			return;
		}

		FBulletInitParams InitParams;
		InitParams.Owner = GetOwner();
		InitParams.SpawnTransform = BuildSpawnTransform(MeshComp, SpawnSocketName);

		// Sim proxy needs collision to drive local hit VFX. Damage/GE must still be server-authoritative.
		if (bForceCollisionOnSimProxy)
		{
			InitParams.CollisionEnabledOverride = 1;
		}

		const int32 InstanceId = BulletSystem->SpawnBullet(BulletID, InitParams);
		if (SlotIndex != INDEX_NONE && InstanceId != INDEX_NONE)
		{
			SlotToInstanceId.Add(SlotIndex, InstanceId);
		}
		return;
	}

	// Authority/autonomous route to GA so GA can inject payload before spawning.
	if (ShouldRouteToGA())
	{
		if (!SpawnEventTag.IsValid())
		{
			UE_LOG(LogAct, Warning, TEXT("ActBulletComponent: SpawnBullet EventTag invalid (will not spawn). Owner=%s BulletID=%s SlotIndex=%d"),
				*GetNameSafe(GetOwner()),
				*BulletID.ToString(),
				SlotIndex);
			return;
		}

		SendBulletGameplayEvent(
			SpawnEventTag,
			SlotIndex,
			BulletID,
			NAME_None,
			/*bResetHitActorsBefore*/ true,
			/*bApplyCollisionResponse*/ true,
			/*DestroyReason*/ EBulletDestroyReason::External,
			/*bSpawnChildren*/ true);
	}
}

void UActBulletComponent::Notify_ProcessManualHits(int32 SlotIndex, FGameplayTag EventTag, bool bResetHitActorsBefore, bool bApplyCollisionResponse)
{
	if (IsSimulatedProxy())
	{
		UBulletSystemComponent* BulletSystem = FindBulletSystemComponent();
		if (!BulletSystem)
		{
			UE_LOG(LogAct, Warning, TEXT("ActBulletComponent: SimProxy ProcessManualHits failed (BulletSystemComponent missing). Owner=%s SlotIndex=%d"),
				*GetNameSafe(GetOwner()),
				SlotIndex);
			return;
		}

		const int32* InstanceIdPtr = SlotToInstanceId.Find(SlotIndex);
		if (!InstanceIdPtr || *InstanceIdPtr == INDEX_NONE)
		{
			return;
		}

		(void)BulletSystem->ProcessManualHits(*InstanceIdPtr, bResetHitActorsBefore, bApplyCollisionResponse);
		return;
	}

	if (ShouldRouteToGA())
	{
		if (!EventTag.IsValid())
		{
			UE_LOG(LogAct, Warning, TEXT("ActBulletComponent: ProcessManualHits EventTag invalid (will not route). Owner=%s SlotIndex=%d"),
				*GetNameSafe(GetOwner()),
				SlotIndex);
			return;
		}

		SendBulletGameplayEvent(
			EventTag,
			SlotIndex,
			NAME_None,
			NAME_None,
			bResetHitActorsBefore,
			bApplyCollisionResponse,
			/*DestroyReason*/ EBulletDestroyReason::External,
			/*bSpawnChildren*/ true);
	}
}

void UActBulletComponent::Notify_DestroyBullet(int32 SlotIndex, FGameplayTag EventTag, EBulletDestroyReason Reason, bool bSpawnChildren)
{
	if (IsSimulatedProxy())
	{
		UBulletSystemComponent* BulletSystem = FindBulletSystemComponent();
		if (!BulletSystem)
		{
			UE_LOG(LogAct, Warning, TEXT("ActBulletComponent: SimProxy DestroyBullet failed (BulletSystemComponent missing). Owner=%s SlotIndex=%d"),
				*GetNameSafe(GetOwner()),
				SlotIndex);
			return;
		}

		const int32* InstanceIdPtr = SlotToInstanceId.Find(SlotIndex);
		if (!InstanceIdPtr || *InstanceIdPtr == INDEX_NONE)
		{
			return;
		}

		(void)BulletSystem->DestroyBullet(*InstanceIdPtr, Reason, bSpawnChildren);
		SlotToInstanceId.Remove(SlotIndex);
		return;
	}

	if (ShouldRouteToGA())
	{
		if (!EventTag.IsValid())
		{
			UE_LOG(LogAct, Warning, TEXT("ActBulletComponent: DestroyBullet EventTag invalid (will not route). Owner=%s SlotIndex=%d Reason=%d"),
				*GetNameSafe(GetOwner()),
				SlotIndex,
				static_cast<int32>(Reason));
			return;
		}

		SendBulletGameplayEvent(
			EventTag,
			SlotIndex,
			NAME_None,
			NAME_None,
			/*bResetHitActorsBefore*/ true,
			/*bApplyCollisionResponse*/ true,
			Reason,
			bSpawnChildren);
	}
}

void UActBulletComponent::NotifyState_BeginSpawnBullet(USkeletalMeshComponent* MeshComp, FName BulletID, FName SpawnSocketName, FName NotifyId, FGameplayTag SpawnEventTag, bool bForceCollisionOnSimProxy)
{
	if (BulletID.IsNone() || NotifyId.IsNone())
	{
		return;
	}

	if (IsSimulatedProxy())
	{
		UBulletSystemComponent* BulletSystem = FindBulletSystemComponent();
		if (!BulletSystem)
		{
			UE_LOG(LogAct, Warning, TEXT("ActBulletComponent: SimProxy NotifyState Begin failed (BulletSystemComponent missing). Owner=%s BulletID=%s NotifyId=%s"),
				*GetNameSafe(GetOwner()),
				*BulletID.ToString(),
				*NotifyId.ToString());
			return;
		}

		FBulletInitParams InitParams;
		InitParams.Owner = GetOwner();
		InitParams.SpawnTransform = BuildSpawnTransform(MeshComp, SpawnSocketName);

		if (bForceCollisionOnSimProxy)
		{
			InitParams.CollisionEnabledOverride = 1;
		}

		const int32 InstanceId = BulletSystem->SpawnBullet(BulletID, InitParams);
		if (InstanceId != INDEX_NONE)
		{
			NotifyIdToInstanceId.Add(NotifyId, InstanceId);
		}
		return;
	}

	if (ShouldRouteToGA())
	{
		if (!SpawnEventTag.IsValid())
		{
			UE_LOG(LogAct, Warning, TEXT("ActBulletComponent: NotifyState Begin SpawnEventTag invalid (will not route). Owner=%s BulletID=%s NotifyId=%s"),
				*GetNameSafe(GetOwner()),
				*BulletID.ToString(),
				*NotifyId.ToString());
			return;
		}

		SendBulletGameplayEvent(
			SpawnEventTag,
			/*SlotIndex*/ INDEX_NONE,
			BulletID,
			NotifyId,
			/*bResetHitActorsBefore*/ true,
			/*bApplyCollisionResponse*/ true,
			/*DestroyReason*/ EBulletDestroyReason::External,
			/*bSpawnChildren*/ true);
	}
}

void UActBulletComponent::NotifyState_EndDestroyBullet(FName NotifyId, FGameplayTag EndEventTag, EBulletDestroyReason Reason, bool bSpawnChildren)
{
	if (NotifyId.IsNone())
	{
		return;
	}

	if (IsSimulatedProxy())
	{
		UBulletSystemComponent* BulletSystem = FindBulletSystemComponent();
		if (!BulletSystem)
		{
			return;
		}

		const int32* InstanceIdPtr = NotifyIdToInstanceId.Find(NotifyId);
		if (!InstanceIdPtr || *InstanceIdPtr == INDEX_NONE)
		{
			return;
		}

		(void)BulletSystem->DestroyBullet(*InstanceIdPtr, Reason, bSpawnChildren);
		NotifyIdToInstanceId.Remove(NotifyId);
		return;
	}

	if (ShouldRouteToGA())
	{
		if (!EndEventTag.IsValid())
		{
			UE_LOG(LogAct, Warning, TEXT("ActBulletComponent: NotifyState End EventTag invalid (will not route). Owner=%s NotifyId=%s Reason=%d"),
				*GetNameSafe(GetOwner()),
				*NotifyId.ToString(),
				static_cast<int32>(Reason));
			return;
		}

		SendBulletGameplayEvent(
			EndEventTag,
			/*SlotIndex*/ INDEX_NONE,
			NAME_None,
			NotifyId,
			/*bResetHitActorsBefore*/ true,
			/*bApplyCollisionResponse*/ true,
			Reason,
			bSpawnChildren);

		// Mapping is only needed for this begin/end window; clear it after routing.
		NotifyIdToInstanceId.Remove(NotifyId);
	}
}

void UActBulletComponent::SetInstanceIdForSlot(int32 SlotIndex, int32 InstanceId)
{
	if (SlotIndex == INDEX_NONE)
	{
		return;
	}
	SlotToInstanceId.Add(SlotIndex, InstanceId);
}

int32 UActBulletComponent::GetInstanceIdForSlot(int32 SlotIndex) const
{
	if (const int32* Found = SlotToInstanceId.Find(SlotIndex))
	{
		return *Found;
	}
	return INDEX_NONE;
}

bool UActBulletComponent::ClearSlot(int32 SlotIndex)
{
	return SlotToInstanceId.Remove(SlotIndex) > 0;
}

void UActBulletComponent::SetInstanceIdForNotifyId(FName NotifyId, int32 InstanceId)
{
	if (NotifyId.IsNone())
	{
		return;
	}
	NotifyIdToInstanceId.Add(NotifyId, InstanceId);
}

int32 UActBulletComponent::GetInstanceIdForNotifyId(FName NotifyId) const
{
	if (const int32* Found = NotifyIdToInstanceId.Find(NotifyId))
	{
		return *Found;
	}
	return INDEX_NONE;
}

bool UActBulletComponent::ClearNotifyId(FName NotifyId)
{
	return NotifyIdToInstanceId.Remove(NotifyId) > 0;
}
