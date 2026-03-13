// Copyright

#include "Animation/AN_AbilityChainReset.h"

#include "Player/ActPlayerController.h"
#include "GameFramework/Pawn.h"

void UAN_AbilityChainReset::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	const APawn* Pawn = MeshComp ? Cast<APawn>(MeshComp->GetOwner()) : nullptr;
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	AActPlayerController* ActPC = Cast<AActPlayerController>(Pawn->GetController());
	if (!ActPC)
	{
		return;
	}

	ActPC->ClearAbilityChainCache();
}
