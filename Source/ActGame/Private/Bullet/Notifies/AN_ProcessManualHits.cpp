// Copyright

#include "Bullet/Notifies/AN_ProcessManualHits.h"

#include "Bullet/ActBulletComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAN_ProcessManualHits::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor)
	{
		return;
	}

	if (UActBulletComponent* BulletComp = OwnerActor->FindComponentByClass<UActBulletComponent>())
	{
		BulletComp->Notify_ProcessManualHits(SlotIndex, EventTag, bResetHitActorsBefore, bApplyCollisionResponse);
	}
}

