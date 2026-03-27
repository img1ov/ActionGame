#include "AbilitySystem/AbilityChain/ActAbilityChainNotifies.h"

#include "AbilitySystem/AbilityChain/ActAbilityChainNotifyUtils.h"
#include "Misc/Crc.h"

void UANS_AbilityChainWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	(void)Animation;
	(void)TotalDuration;
	(void)EventReference;

	if (UActAbilitySystemComponent* ActASC = ActAbilityChainNotifyUtils::ResolveActASC(MeshComp))
	{
		ActASC->OpenAbilityChainWindow(BuildWindowDefinition());
	}
}

void UANS_AbilityChainWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	(void)Animation;
	(void)EventReference;

	if (UActAbilitySystemComponent* ActASC = ActAbilityChainNotifyUtils::ResolveActASC(MeshComp))
	{
		ActASC->CloseAbilityChainWindow(GetWindowId());
	}
}

FActAbilityChainWindowDefinition UANS_AbilityChainWindow::BuildWindowDefinition() const
{
	// The authored notify owns its entire combo window definition.
	FActAbilityChainWindowDefinition WindowDefinition;
	WindowDefinition.WindowId = GetWindowId();
	WindowDefinition.WindowPriority = WindowPriority;
	WindowDefinition.Entries = Entries;
	return WindowDefinition;
}

FName UANS_AbilityChainWindow::GetWindowId() const
{
	// Hashing the notify object path yields a deterministic per-placement runtime Id
	// without introducing another manually-maintained asset field.
	const uint32 PathHash = FCrc::StrCrc32(*GetPathName());
	return FName(*FString::Printf(TEXT("AbilityChainWindow_%08X"), PathHash));
}
