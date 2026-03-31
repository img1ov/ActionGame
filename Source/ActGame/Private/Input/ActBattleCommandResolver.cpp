// Copyright

#include "Input/ActBattleCommandResolver.h"

#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "Character/ActComboGraphComponent.h"

namespace
{
double GetResolverTimeSeconds(UActAbilitySystemComponent& ActASC)
{
	const UWorld* World = ActASC.GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}
}

void FActBattleCommandResolver::Reset()
{
}

EActBattleCommandResolveResult FActBattleCommandResolver::ResolveCommand(
	UActAbilitySystemComponent& ActASC,
	const FGameplayTag& CommandTag,
	bool& bOutShouldConsumeCommand)
{
	bOutShouldConsumeCommand = true;
	if (!CommandTag.IsValid())
	{
		return EActBattleCommandResolveResult::NotHandled;
	}

	if (UActComboGraphComponent* ComboGraphComp = UActComboGraphComponent::FindComboGraphComponent(ActASC.GetAvatarActor()))
	{
		if (ComboGraphComp->TryHandleCommand(CommandTag))
		{
			UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleCommand] ComboGraph handled. Owner=%s Command=%s Active=%d Time=%.3f"),
				*GetNameSafe(ActASC.GetAvatarActor()),
				*CommandTag.ToString(),
				ComboGraphComp->IsComboGraphActive(),
				GetResolverTimeSeconds(ActASC));
			return EActBattleCommandResolveResult::Activated;
		}

		if (ComboGraphComp->IsComboGraphActive())
		{
			return EActBattleCommandResolveResult::NotHandled;
		}
	}

	const bool bActivated = ActASC.TryActivateGrantedAbilityByInputTag(
		CommandTag,
		/*bAllowRemoteActivation=*/true,
		/*bCancelIfAlreadyActive=*/false,
		nullptr,
		nullptr);

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleCommand] Resolver starter fallback. Owner=%s Command=%s Activated=%d Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*CommandTag.ToString(),
		bActivated,
		GetResolverTimeSeconds(ActASC));

	return bActivated ? EActBattleCommandResolveResult::Activated : EActBattleCommandResolveResult::NotHandled;
}
