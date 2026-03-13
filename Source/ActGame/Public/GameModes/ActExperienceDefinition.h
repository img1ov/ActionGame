// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataAsset.h"
#include "ActExperienceDefinition.generated.h"

class UActExperienceActionSet;
class UGameFeatureAction;
class UActPawnData;

/**
 * Definition of an experience
 */
UCLASS(BlueprintType, Const)
class ACTGAME_API UActExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UActExperienceDefinition();

public:
	// List of Game Feature Plugins this experience wants to have active
	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TArray<FString> GameFeaturesToEnable;

	/** The default pawn class to spawn for players */
	//@TODO: Make soft?
	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TObjectPtr<UActPawnData> DefaultPawnData;

	// List of actions to perform as this experience is loaded/activated/deactivated/unloaded
	UPROPERTY(EditDefaultsOnly, Instanced, Category="Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	// List of additional action sets to compose into this experience
	UPROPERTY(EditDefaultsOnly, Category=Gameplay)
	TArray<TObjectPtr<UActExperienceActionSet>> ActionSets;
};
