// Copyright

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ActAbilityChainTypes.generated.h"

USTRUCT(BlueprintType)
struct FActAbilityChainEntry
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (Categories = "InputCommand"))
	FGameplayTag CommandTag;

	// AbilityID defined on UActGameplayAbility.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	FName AbilityID;
};
