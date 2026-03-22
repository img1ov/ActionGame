// Copyright

#include "Bullet/Notifies/ANS_SpawnBullet.h"

#include "Bullet/ActBulletComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/Guid.h"

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

void UANS_SpawnBullet::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor)
	{
		return;
	}

	if (UActBulletComponent* BulletComp = OwnerActor->FindComponentByClass<UActBulletComponent>())
	{
		BulletComp->NotifyState_BeginSpawnBullet(MeshComp, BulletID, SpawnSocketName, NotifyId, SpawnEventTag, bForceCollisionOnSimProxy);
	}
}

void UANS_SpawnBullet::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor)
	{
		return;
	}

	if (UActBulletComponent* BulletComp = OwnerActor->FindComponentByClass<UActBulletComponent>())
	{
		BulletComp->NotifyState_EndDestroyBullet(NotifyId, EndEventTag, EndReason, bSpawnChildrenOnEnd);
	}
}
