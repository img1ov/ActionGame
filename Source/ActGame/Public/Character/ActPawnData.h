// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "ActPawnData.generated.h"

class APawn;
class UActInputCommandConfig;
class UActInputConfig;
class UActAbilityTagRelationshipMapping;
class UActAbilitySet;

UCLASS()
class ACTGAME_API UActPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	
	UActPawnData(const FObjectInitializer& ObjectInitializer);

	// Class to instantiate for this pawn (should usually derive from ALyraPawn or ALyraCharacter).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Act|Pawn")
	TSubclassOf<APawn> PawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Act|Abilities")
	TArray<TObjectPtr<UActAbilitySet>> AbilitySets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Act|Abilities")
	TObjectPtr<UActAbilityTagRelationshipMapping> TagRelationshipMapping;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Act|Input")
	TObjectPtr<UActInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Act|Input")
	TObjectPtr<UActInputCommandConfig> InputCommandConfig;
};
