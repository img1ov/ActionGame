// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemInterface.h"
#include "Input/ActBattleInputAnalyzer.h"
#include "Input/ActCommandRuntimeResolver.h"
#include "ModularPlayerController.h"

#include "ActPlayerController.generated.h"

class UStateTree;
class UActStateTreeComponent;
class AActPlayerState;
class UAbilitySystemComponent;
class UActAbilitySystemComponent;
class UObject;
struct FFrame;
struct FGameplayEventData;

class FActBattleInputAnalyzer;
DECLARE_MULTICAST_DELEGATE_OneParam(FActInputCommandMatched, const FGameplayTag&);

/**
 * Input orchestration point for local player:
 * - receives raw input events from HeroComponent,
 * - feeds analyzer,
 * - consumes matched command tags,
 * - optionally resolves combo derivations,
 * - then forwards final trigger tags to ASC.
 */
UCLASS()
class ACTGAME_API AActPlayerController :  public AModularPlayerController, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	
	AActPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual ~AActPlayerController() override;

	UFUNCTION(BlueprintCallable, Category = "Act|PlayerController")
	AActPlayerState* GetActPlayerState() const;
	
	UFUNCTION(BlueprintCallable, Category = "Act|PlayerController")
	UActAbilitySystemComponent* GetActAbilitySystemComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Act|PlayerController")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//~APlayerController interface
	virtual void PreProcessInput(const float DeltaTime, const bool bGamePaused) override;
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	virtual void PlayerTick(float DeltaTime) override;
	//~End of APlayerController interface

	/** Push one pressed input tag into runtime analyzer history. */
	void PushInputTagPressedToAnalyzer(const FGameplayTag& InputTag) const;

	/** Queue an ability input tag press; will be routed after command matching. */
	void QueueAbilityInputTagPressed(const FGameplayTag& InputTag);

	/** Queue an ability input tag release; will be routed after command matching. */
	void QueueAbilityInputTagReleased(const FGameplayTag& InputTag);

	/** Push normalized movement direction into runtime analyzer history. */
	void PushDirectionToAnalyzer(EInputDirection InputDirection);

	/** Replace command definitions used by analyzer. */
	void ConfigureInputCommandDefinitions(const TArray<FInputCommandDefinition>& InCommandDefinitions);

	/** Runtime command matched event. */
	FActInputCommandMatched& OnInputCommandMatched() { return InputCommandMatched; }

	/** Debug/inspection only: analyzer internals are not mutation API. */
	const FActBattleInputAnalyzer* GetInputAnalyzer() const { return BattleInputAnalyzer.Get(); }

protected:
	void HandleInputCommandMatched(const FGameplayTag& CommandTag) const;
	void BuildAnalyzerStateTags(FGameplayTagContainer& OutStateTags) const;
	double GetAnalyzerCurrentTimeSeconds() const;

public:
	
	void RegisterAbilityChainWindow(FName WindowId, const TArray<FActAbilityChainEntry>& ChainEntries);
	void UnregisterAbilityChainWindow(FName WindowId);
	void ClearAbilityChainCache();

private:
	TUniquePtr<FActBattleInputAnalyzer> BattleInputAnalyzer;
	TUniquePtr<FActCommandRuntimeResolver> ComboRuntime;
	FActInputCommandMatched InputCommandMatched;
	EInputDirection CurrentAnalyzerDirection = EInputDirection::Neutral;
	TArray<FGameplayTag> PendingAbilityInputPressed;
	TArray<FGameplayTag> PendingAbilityInputReleased;
	bool bCommandMatchedThisFrame = false;
};
