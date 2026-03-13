// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/WorldSettings.h"
#include "ActWorldSettings.generated.h"

class UActExperienceDefinition;
/**
 * 
 */
UCLASS()
class ACTGAME_API AActWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	AActWorldSettings(const FObjectInitializer& ObjectInitializer);

public:
	FPrimaryAssetId GetDefaultGameplayExperience() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = GameMode)
	TSoftClassPtr<UActExperienceDefinition> DefaultGameplayExperience;
};
