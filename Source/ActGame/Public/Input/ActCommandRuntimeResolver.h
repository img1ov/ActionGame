// Copyright

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/AbilityChain/ActAbilityChainTypes.h"

class UActAbilitySystemComponent;

/**
 * Runtime combo resolver.
 * Owns transient combo context only; all authoring remains in AnimNotify windows.
 */
class ACTGAME_API FActCommandRuntimeResolver
{
public:
	
	void Reset();
	void ClearAbilityChainWindows();
	void ClearCachedAbility();

	void RegisterAbilityChainWindow(const FName WindowId, const TArray<FActAbilityChainEntry>& ChainEntries);
	void UnregisterAbilityChainWindow(const FName WindowId);
	bool HasActiveAbilityChains() const;

	/** Resolve and execute one command. Returns true when one ability activation succeeded. */
	bool TryExecuteCommand(UActAbilitySystemComponent& ActASC, const FGameplayTag& CommandTag, const FGameplayTagContainer& OwnerTags);

private:
	
	struct FActAbilityChainRuntimeEntry
	{
		FGameplayTag CommandTag;
		FName AbilityID;
		FName WindowId;
	};

private:
	
	TMap<FName, TArray<FActAbilityChainEntry>> ActiveChainWindows;
	TMap<FGameplayTag, TArray<FActAbilityChainRuntimeEntry>> ActiveChainByCommand;
	FGameplayAbilitySpecHandle CachedAbilityHandle;
};
