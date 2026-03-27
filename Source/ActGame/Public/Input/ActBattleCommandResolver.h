// Copyright

#pragma once

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
	 * - If idle, we may still activate starter abilities by matching the command tag directly.
	 *
	 * bOutShouldConsumeCommand:
	 * - True when the command should be removed from input buffer after successful handling.
	 */
	EActBattleCommandResolveResult ResolveCommand(UActAbilitySystemComponent& ActASC, const FGameplayTag& CommandTag, bool& bOutShouldConsumeCommand);
};
