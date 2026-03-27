#include "Character/ActCharacterNotifies.h"

#include "Character/ActHeroComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_MovementInputBlock::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	(void)Animation;
	(void)TotalDuration;
	(void)EventReference;

	if (!MeshComp)
	{
		return;
	}

	if (UActHeroComponent* HeroComponent = UActHeroComponent::FindHeroComponent(MeshComp->GetOwner()))
	{
		HeroComponent->PushMovementInputBlock(this);
	}
}

void UANS_MovementInputBlock::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	(void)Animation;
	(void)EventReference;

	if (!MeshComp)
	{
		return;
	}

	if (UActHeroComponent* HeroComponent = UActHeroComponent::FindHeroComponent(MeshComp->GetOwner()))
	{
		HeroComponent->PopMovementInputBlock(this);
	}
}
