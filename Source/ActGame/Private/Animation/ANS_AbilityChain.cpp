// Copyright

#include "Animation/ANS_AbilityChain.h"

#include "Player/ActPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/Guid.h"

void UANS_AbilityChain::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (WindowId.IsNone())
	{
		WindowId = FName(*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
	}
}

void UANS_AbilityChain::PostLoad()
{
	Super::PostLoad();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (WindowId.IsNone())
	{
		WindowId = FName(*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
	}
}

void UANS_AbilityChain::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	WindowId = FName(*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
}

void UANS_AbilityChain::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (ChainEntries.IsEmpty())
	{
		return;
	}

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

	ActPC->RegisterAbilityChainWindow(WindowId, ChainEntries);
}

void UANS_AbilityChain::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

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

	ActPC->UnregisterAbilityChainWindow(WindowId);
}
