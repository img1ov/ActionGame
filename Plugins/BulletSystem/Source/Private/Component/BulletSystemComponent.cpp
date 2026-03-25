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

int32 UBulletSystemComponent::ProcessManualHits(int32 InstanceId, bool bApplyCollisionResponse)
{
	return ProcessManualHitsInternal(InstanceId, bApplyCollisionResponse);
}

int32 UBulletSystemComponent::ProcessManualHitsInternal(int32 InstanceId, bool bApplyCollisionResponse) const
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

	return Subsystem->GetController()->ProcessManualHits(InstanceId, bApplyCollisionResponse);
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
	if (InstanceKey.IsNone())
	{
		return INDEX_NONE;
	}

	TArray<int32>* InstanceIds = InstanceRegistry.InstanceKeyMap.Find(InstanceKey);
	if (!InstanceIds || InstanceIds->Num() == 0)
	{
		return INDEX_NONE;
	}

	// Prune invalid entries from the back and return the newest still-valid instance.
	for (int32 Index = InstanceIds->Num() - 1; Index >= 0; --Index)
	{
		const int32 Candidate = (*InstanceIds)[Index];
		if (IsInstanceIdValid(Candidate))
		{
			if (Index != InstanceIds->Num() - 1)
			{
				InstanceIds->SetNum(Index + 1);
			}
			return Candidate;
		}

		InstanceIds->RemoveAt(Index, /*Count*/ 1, EAllowShrinking::No);
	}

	InstanceRegistry.InstanceKeyMap.Remove(InstanceKey);
	return INDEX_NONE;
}

int32 UBulletSystemComponent::ConsumeOldestInstanceIdByKey(FName InstanceKey)
{
	if (InstanceKey.IsNone())
	{
		return INDEX_NONE;
	}

	TArray<int32>* InstanceIds = InstanceRegistry.InstanceKeyMap.Find(InstanceKey);
	if (!InstanceIds || InstanceIds->Num() == 0)
	{
		return INDEX_NONE;
	}

	while (InstanceIds->Num() > 0)
	{
		const int32 Candidate = (*InstanceIds)[0];
		InstanceIds->RemoveAt(0, /*Count*/ 1, EAllowShrinking::No);
		if (IsInstanceIdValid(Candidate))
		{
			if (InstanceIds->Num() == 0)
			{
				InstanceRegistry.InstanceKeyMap.Remove(InstanceKey);
			}
			return Candidate;
		}
	}

	InstanceRegistry.InstanceKeyMap.Remove(InstanceKey);
	return INDEX_NONE;
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
