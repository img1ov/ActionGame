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

	// When multiple chain entries match the same command, higher Priority wins.
	// Tie-breaker: most recently registered window / later entry wins.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (ClampMin = "0"))
	int32 Priority = 0;
};
