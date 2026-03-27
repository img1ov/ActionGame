#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "AbilitySystem/AbilityChain/ActAbilityChainTypes.h"
#include "ActAbilityChainData.generated.h"

UCLASS(BlueprintType)
class ACTGAME_API UActAbilityChainData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityChain")
	FName StartNodeId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityChain", meta = (ClampMin = "0.0"))
	float PredictionGraceSeconds = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityChain", meta = (TitleProperty = "NodeId"))
	TArray<FActAbilityChainNode> Nodes;

	const FActAbilityChainNode* FindNode(FName NodeId) const;
	const FActAbilityChainNode* GetStartNode() const;
	FName GetStartSectionName() const;
};
