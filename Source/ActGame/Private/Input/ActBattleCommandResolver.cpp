// Copyright

#include "Input/ActBattleCommandResolver.h"

#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "AbilitySystem/AbilityChain/ActAbilityChainRuntime.h"

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

	const FActAbilityChainCommandResolveResult ChainResult = ActASC.ResolveAbilityChainCommand(CommandTag);
	if (ChainResult.WasHandled())
	{
		// All combo authority lives in ASC; the input layer only consumes the resolved command.
		bOutShouldConsumeCommand = ChainResult.bConsumeInput;
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleCommand] Chain resolved. Owner=%s Command=%s Mode=%d SourceAbilityId=%s TargetAbilityId=%s Time=%.3f"),
			*GetNameSafe(ActASC.GetAvatarActor()),
			*CommandTag.ToString(),
			static_cast<int32>(ChainResult.Resolution),
			*ChainResult.SourceAbilityId.ToString(),
			*ChainResult.TargetAbilityId.ToString(),
			GetResolverTimeSeconds(ActASC));
		return EActBattleCommandResolveResult::Activated;
	}

	if (ActASC.HasActiveAbilityChain() || ActASC.GetAnimatingAbility() != nullptr)
	{
		// If something combat-related is active, we do not attempt "starter fallback" here.
		// The chain runtime is the only place that can decide whether a transition is allowed.
		return EActBattleCommandResolveResult::NotHandled;
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
