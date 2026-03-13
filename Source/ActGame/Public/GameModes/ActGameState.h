// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ModularGameState.h"
#include "AbilitySystemInterface.h"

#include "ActGameState.generated.h"

class UActAbilitySystemComponent;
class UActExperienceManagerComponent;
class UObject;
struct FFrame;

/**
 * 
 */
UCLASS()
class ACTGAME_API AActGameState : public AModularGameStateBase, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AActGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	virtual void Tick(float DeltaSeconds) override;
	//~End of AActor interface

	// Gets the server's FPS, replicated to clients
	float GetServerFPS() const;

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	UFUNCTION(BlueprintCallable, Category = "Act|GameState")
	UActAbilitySystemComponent* GetActAbilitySystemComponent() const { return AbilitySystemComponent; }

protected:
	UPROPERTY(Replicated)
	float ServerFPS;
	
private:
	UPROPERTY()
	TObjectPtr<UActExperienceManagerComponent> ExperienceManagerComponent;

	UPROPERTY(VisibleAnywhere, Category = "Act|GameState")
	TObjectPtr<UActAbilitySystemComponent> AbilitySystemComponent;
};
