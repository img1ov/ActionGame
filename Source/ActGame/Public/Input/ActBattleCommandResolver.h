// Copyright

#pragma once

#include "AbilitySystem/AbilityChain/ActAbilityChainTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UActAbilitySystemComponent;

enum class EActBattleCommandResolveResult : uint8
{
	NotHandled,
	Activated
};

/**
 * Input-domain resolver.
 * Command matching stays in the input module, while all chain state lives in the ASC.
 */
class ACTGAME_API FActBattleCommandResolver
{
public:
	
	/** Clears any internal state (currently stateless, but kept for symmetry/extension). */
	void Reset();

	/**
	 * Resolves one command against the ASC:
	 * - If there is an active chain, we ask the chain runtime to resolve the command.
	 * - If idle, we may still activate starter abilities depending on chain activation modes.
	 *
	 * OutServerRelayTransition:
	 * - Filled when a client predicted a node jump or ability transition that must be applied on server.
	 * - The controller typically relays this to server via RPC so the server can follow the same branch.
	 *
	 * bOutShouldConsumeCommand:
	 * - True when the command should be removed from input buffer after successful handling.
	 */
	EActBattleCommandResolveResult ResolveCommand(UActAbilitySystemComponent& ActASC, const FGameplayTag& CommandTag, FActAbilityChainReplicatedTransition& OutServerRelayTransition, bool& bOutShouldConsumeCommand);

	/**
	 * Applies a server-authoritative transition on the client.
	 * This is used to catch up client state after prediction or when the server corrects a branch.
	 */
	void ApplyReplicatedTransition(UActAbilitySystemComponent& ActASC, const FActAbilityChainReplicatedTransition& Transition);
};
