// BulletSystem: BulletSystemNotifies.cpp

#include "Extension/BulletSystemNotifies.h"

#include "Extension/BulletNotifyExecution.h"

#include "Components/SkeletalMeshComponent.h"

UAN_SpawnBullet::UAN_SpawnBullet()
{
	// Treat gameplay-critical notifies as native branching points when used on montages.
	// This avoids missing single-frame queued notifies on the server when montage evaluation skips/fast-forwards.
	bIsNativeBranchingPoint = true;
}

void UAN_SpawnBullet::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	OnNotify(MeshComp, Animation, EventReference);
}

void UAN_SpawnBullet::OnNotify_Implementation(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) const
{
	(void)Animation;
	(void)EventReference;
	BulletNotifyExecution::HandleSpawnNotify(*this, MeshComp);
}

UAN_ProcessManualHits::UAN_ProcessManualHits()
{
	bIsNativeBranchingPoint = true;
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
	BulletNotifyExecution::HandleProcessManualHitsNotify(*this, MeshComp);
}

UAN_DestroyBullet::UAN_DestroyBullet()
{
	bIsNativeBranchingPoint = true;
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
	BulletNotifyExecution::HandleDestroyNotify(*this, MeshComp);
}

UANS_SpawnBullet::UANS_SpawnBullet()
{
	bIsNativeBranchingPoint = true;
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
	BulletNotifyExecution::HandleSpawnStateBegin(*this, MeshComp);
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
	BulletNotifyExecution::HandleSpawnStateEnd(*this, MeshComp);
}
