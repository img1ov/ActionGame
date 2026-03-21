// BulletSystem: BulletSystemComponent.cpp
#include "Component/BulletSystemComponent.h"

#include "BulletLogChannels.h"
#include "BulletSystemTypes.h"
#include "Config/BulletConfig.h"
#include "Controller/BulletController.h"
#include "Controller/BulletWorldSubsystem.h"

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

	if (BulletConfig)
	{
		BulletConfig->RebuildRuntimeTable();
	}
}

int32 UBulletSystemComponent::SpawnBullet(FName BulletID, const FBulletInitParams& InitParams)
{
	return SpawnBulletInternal(BulletID, InitParams);
}

int32 UBulletSystemComponent::SpawnBulletInternal(FName BulletID, const FBulletInitParams& InitParams)
{
	if (BulletID.IsNone())
	{
		return INDEX_NONE;
	}

	if (!BulletConfig)
	{
		UE_LOG(LogBullet, Warning, TEXT("BulletSystemComponent: SpawnBullet failed, BulletConfig missing. Owner=%s BulletID=%s"),
			*GetNameSafe(GetOwner()),
			*BulletID.ToString());
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
	Subsystem->GetController()->SpawnBullet(InitParams, BulletID, InstanceId, BulletConfig);
	return InstanceId;
}

bool UBulletSystemComponent::DestroyBullet(int32 InstanceId, EBulletDestroyReason Reason, bool bSpawnChildren)
{
	return DestroyBulletInternal(InstanceId, Reason, bSpawnChildren);
}

bool UBulletSystemComponent::DestroyBulletInternal(int32 InstanceId, EBulletDestroyReason Reason, bool bSpawnChildren) const
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

	Subsystem->GetController()->RequestDestroyBullet(InstanceId, Reason, bSpawnChildren);
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
