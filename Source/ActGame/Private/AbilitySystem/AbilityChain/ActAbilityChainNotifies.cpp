#include "AbilitySystem/AbilityChain/ActAbilityChainNotifies.h"

#include "AbilitySystem/AbilityChain/ActAbilityChainNotifyUtils.h"

void UAN_AbilityChainNode::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	(void)Animation;
	(void)EventReference;

	if (NodeId.IsNone())
	{
		return;
	}

	if (UActAbilitySystemComponent* ActASC = ActAbilityChainNotifyUtils::ResolveActASC(MeshComp))
	{
		ActASC->EnterAbilityChainNode(NodeId);
	}
}

void UANS_AbilityChainWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	(void)Animation;
	(void)TotalDuration;
	(void)EventReference;

	if (!WindowTag.IsValid())
	{
		return;
	}

	if (UActAbilitySystemComponent* ActASC = ActAbilityChainNotifyUtils::ResolveActASC(MeshComp))
	{
		ActASC->OpenAbilityChainWindow(WindowTag, this);
	}
}

void UANS_AbilityChainWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	(void)Animation;
	(void)EventReference;

	if (!WindowTag.IsValid())
	{
		return;
	}

	if (UActAbilitySystemComponent* ActASC = ActAbilityChainNotifyUtils::ResolveActASC(MeshComp))
	{
		ActASC->CloseAbilityChainWindow(WindowTag, this);
	}
}
