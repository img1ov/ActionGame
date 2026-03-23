// BulletSystem: BulletSystemComponent.cpp
#include "BulletSystemComponent.h"

#include "BulletLogChannels.h"
#include "BulletSystemTypes.h"
#include "Config/BulletConfig.h"
#include "Controller/BulletController.h"
#include "Controller/BulletWorldSubsystem.h"
#include "Model/BulletModel.h"
#include "Model/BulletInfo.h"

#include "Engine/World.h"

UBulletSystemComponent::UBulletSystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(false);
	PrimaryComponentTick.bCanEverTick = false;
}

void UBulletSystemComponent::SetBulletConfig(UBulletConfig* InConfig)
{
	if (BulletConfig == InConfig)
	{
		return;
	}

	BulletConfig = InConfig;
}

int32 UBulletSystemComponent::SpawnBullet(FName BulletId, const FBulletInitParams& InitParams)
{
	return SpawnBulletInternal(BulletId, InitParams);
}

int32 UBulletSystemComponent::SpawnBulletInternal(FName BulletId, const FBulletInitParams& InitParams)
{
	if (BulletId.IsNone())
	{
		return INDEX_NONE;
	}

	if (!BulletConfig)
	{
		UE_LOG(LogBullet, Warning, TEXT("BulletSystemComponent: SpawnBullet failed, BulletConfig missing. Owner=%s BulletId=%s"),
			*GetNameSafe(GetOwner()),
			*BulletId.ToString());
		return INDEX_NONE;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return INDEX_NONE;
	}

	UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
	if (!Subsystem || !Subsystem->GetController())
	{
		return INDEX_NONE;
	}

	int32 InstanceId = INDEX_NONE;
	Subsystem->GetController()->SpawnBullet(InitParams, BulletId, InstanceId, BulletConfig);

	if (InstanceId != INDEX_NONE && !InitParams.InstanceKey.IsNone())
	{
		InstanceRegistry.AddToKeyMap(InitParams.InstanceKey, InstanceId);
	}
	return InstanceId;
}

bool UBulletSystemComponent::DestroyBullet(int32 InstanceId)
{
	return DestroyBulletInternal(InstanceId);
}

bool UBulletSystemComponent::DestroyBulletInternal(int32 InstanceId) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
	if (!Subsystem || !Subsystem->GetController())
	{
		return false;
	}

	Subsystem->GetController()->RequestDestroyBullet(InstanceId);
	return true;
}

int32 UBulletSystemComponent::ProcessManualHits(int32 InstanceId, bool bResetHitActorsBefore, bool bApplyCollisionResponse)
{
	return ProcessManualHitsInternal(InstanceId, bResetHitActorsBefore, bApplyCollisionResponse);
}

int32 UBulletSystemComponent::ProcessManualHitsInternal(int32 InstanceId, bool bResetHitActorsBefore, bool bApplyCollisionResponse) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
	if (!Subsystem || !Subsystem->GetController())
	{
		return 0;
	}

	return Subsystem->GetController()->ProcessManualHits(InstanceId, bResetHitActorsBefore, bApplyCollisionResponse);
}

bool UBulletSystemComponent::IsInstanceIdValid(int32 InstanceId) const
{
	if (InstanceId == INDEX_NONE)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
	if (!Subsystem || !Subsystem->GetController())
	{
		return false;
	}

	UBulletModel* Model = Subsystem->GetController()->GetModel();
	if (!Model)
	{
		return false;
	}

	const FBulletInfo* Info = Model->GetBullet(InstanceId);
	return Info != nullptr && !Info->bNeedDestroy;
}

int32 UBulletSystemComponent::GetInstanceIdByKey(FName InstanceKey) const
{
	const int32 InstanceId = InstanceRegistry.GetInstanceIdByKey(InstanceKey);
	if (InstanceId == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (!IsInstanceIdValid(InstanceId))
	{
		// Lazy prune to avoid stale key accumulation (e.g. bullet lifetime expiry).
		InstanceRegistry.InstanceKeyMap.Remove(InstanceKey);
		return INDEX_NONE;
	}

	return InstanceId;
}

void UBulletSystemComponent::SetInstanceIdKey(FName InstanceKey, int32 InstanceId)
{
	InstanceRegistry.AddToKeyMap(InstanceKey, InstanceId);
}

bool UBulletSystemComponent::RemoveInstanceKey(FName InstanceKey)
{
	return InstanceRegistry.RemoveKey(InstanceKey);
}

void UBulletSystemComponent::ClearInstanceKeys()
{
	InstanceRegistry.ClearKeyMap();
}

bool UBulletSystemComponent::IsInstanceKeyValid(FName InstanceKey) const
{
	return GetInstanceIdByKey(InstanceKey) != INDEX_NONE;
}
