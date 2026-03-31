// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "ActComboGraphConfig.generated.h"

class UComboGraph;
class UInputAction;

USTRUCT(BlueprintType)
struct FActComboGraphCommandBinding
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo Graph", meta = (Categories = "InputTag,InputCommand"))
	FGameplayTag CommandTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo Graph")
	TObjectPtr<UComboGraph> ComboGraph = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo Graph")
	TObjectPtr<UInputAction> InputAction = nullptr;

	bool IsValid() const
	{
		return CommandTag.IsValid() && (ComboGraph != nullptr || InputAction != nullptr);
	}
};

UCLASS(BlueprintType, Const)
class ACTGAME_API UActComboGraphConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo Graph", meta = (TitleProperty = "CommandTag"))
	TArray<FActComboGraphCommandBinding> CommandBindings;
};
