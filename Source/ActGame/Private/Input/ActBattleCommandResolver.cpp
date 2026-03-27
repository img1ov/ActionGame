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
	FActAbilityChainReplicatedTransition& OutServerRelayTransition,
	bool& bOutShouldConsumeCommand)
{
	OutServerRelayTransition = FActAbilityChainReplicatedTransition();
	bOutShouldConsumeCommand = true;
	if (!CommandTag.IsValid())
	{
		return EActBattleCommandResolveResult::NotHandled;
	}

	const FActAbilityChainCommandResolveResult ChainResult = ActASC.ResolveAbilityChainCommand(CommandTag, true);
	if (ChainResult.WasHandled())
	{
		// All chain authority lives in ASC; input layer only relays the chosen transition to server.
		// This keeps matching and execution separate and avoids duplicating window logic in input code.
		OutServerRelayTransition = ChainResult.ToReplicatedTransition();
		bOutShouldConsumeCommand = ChainResult.bConsumeInput;
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleCommand] Chain resolved. Owner=%s Command=%s Mode=%d SourceNode=%s AbilityID=%s Node=%s Relay=%d Time=%.3f"),
			*GetNameSafe(ActASC.GetAvatarActor()),
			*CommandTag.ToString(),
			static_cast<int32>(ChainResult.Resolution),
			*ChainResult.SourceNodeId.ToString(),
			*ChainResult.TargetAbilityID.ToString(),
			*ChainResult.TargetNodeId.ToString(),
			OutServerRelayTransition.IsValid(),
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

void FActBattleCommandResolver::ApplyReplicatedTransition(UActAbilitySystemComponent& ActASC, const FActAbilityChainReplicatedTransition& Transition)
{
	if (!Transition.IsValid())
	{
		return;
	}

	const bool bApplied = ActASC.ApplyReplicatedAbilityChainTransition(Transition);
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleCommand] Replicated chain apply. Owner=%s SourceNode=%s TargetAbility=%s TargetNode=%s Applied=%d Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*Transition.SourceNodeId.ToString(),
		*Transition.TargetAbilityID.ToString(),
		*Transition.TargetNodeId.ToString(),
		bApplied,
		GetResolverTimeSeconds(ActASC));
}
