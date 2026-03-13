// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Input/ActBattleInputAnalyzer.h"

#include "ActInputCommandConfig.generated.h"

/**
 * Per-pawn command config.
 * This is the authoring entry for command patterns consumed by FActBattleInputAnalyzer.
 */
UCLASS(BlueprintType, Const)
class ACTGAME_API UActInputCommandConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UActInputCommandConfig(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "OutputCommandTag"))
	TArray<FInputCommandDefinition> CommandDefinitions;
};

