// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemInterface.h"
#include "CommonPlayerController.h"
#include "Input/ActBattleInputAnalyzer.h"
#include "Input/ActBattleCommandResolver.h"
#include "Teams/ActTeamAgentInterface.h"

#include "ActPlayerController.generated.h"

#define UE_API ACTGAME_API

struct FGenericTeamId;

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
 * AActPlayerController
 *
 *	The base player controller class used by this project.
 */
UCLASS(MinimalAPI, Config = Game, Meta = (ShortTooltip = "The base player controller class used by this project."))
class AActPlayerController :  public ACommonPlayerController, public IAbilitySystemInterface, public IActTeamAgentInterface
{
	GENERATED_BODY()

public:
	
	UE_API AActPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	UE_API virtual ~AActPlayerController() override;

	UFUNCTION(BlueprintCallable, Category = "Act|PlayerController")
	UE_API AActPlayerState* GetActPlayerState() const;
	
	UFUNCTION(BlueprintCallable, Category = "Act|PlayerController")
	UE_API UActAbilitySystemComponent* GetActAbilitySystemComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Act|PlayerController")
	UE_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	//~IActTeamAgentInterface interface
	UE_API virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	UE_API virtual FGenericTeamId GetGenericTeamId() const override;
	UE_API virtual FOnActTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	//~End of IActTeamAgentInterface interface

	/** Push one pressed input tag into runtime analyzer history. */
	UE_API void PushInputTagPressedToAnalyzer(const FGameplayTag& InputTag) const;

	/** Queue an ability input tag press; will be routed after command matching. */
	UE_API void QueueAbilityInputTagPressed(const FGameplayTag& InputTag);

	/** Queue an ability input tag release; will be routed after command matching. */
	UE_API void QueueAbilityInputTagReleased(const FGameplayTag& InputTag);

	/** Push normalized movement direction into runtime analyzer history. */
	UE_API void PushDirectionToAnalyzer(EInputDirection InputDirection);

	/** Replace command definitions used by analyzer. */
	UE_API void ConfigureInputCommandDefinitions(const TArray<FInputCommandDefinition>& InCommandDefinitions);

	/** Runtime command matched event. */
	UE_API FActInputCommandMatched& OnInputCommandMatched() { return InputCommandMatched; }

	/** Debug/inspection only: analyzer internals are not mutation API. */
	UE_API const FActBattleInputAnalyzer* GetInputAnalyzer() const { return BattleInputAnalyzer.Get(); }
	
	UE_API void ClearAbilityChainCache();
	
protected:
	
	//~AActor interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of AActor interface
	
	//~AController interface
	UE_API virtual void OnPossess(APawn* InPawn) override;
	UE_API virtual void OnUnPossess() override;
	UE_API virtual void InitPlayerState() override;
	UE_API virtual void CleanupPlayerState() override;
	UE_API virtual void OnRep_PlayerState() override;
	//~End of AController interface
	
	//~APlayerController interface
	UE_API virtual void PreProcessInput(const float DeltaTime, const bool bGamePaused) override;
	UE_API virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	UE_API virtual void PlayerTick(float DeltaTime) override;
	//~End of APlayerController interface
	
	// Called when the player state is set or cleared
	UE_API virtual void OnPlayerStateChanged();
	
	/* Input Analyzer */
	void HandleInputCommandMatched(const FGameplayTag& CommandTag) const;
	void BuildAnalyzerStateTags(FGameplayTagContainer& OutStateTags) const;
	double GetAnalyzerCurrentTimeSeconds() const;
	void ResolveBufferedCommand(UActAbilitySystemComponent& ActASC);
	void DebugPrintCommandExecution(const FGameplayTag& CommandTag) const;
	void ResetCommandInputBuffer();
	/* ~End of Input Analyzer */

private:
	
	UPROPERTY()
	FOnActTeamIndexChangedDelegate OnTeamChangedDelegate;

	UPROPERTY()
	TObjectPtr<APlayerState> LastSeenPlayerState;
	
private:
	
	void BroadcastOnPlayerStateChanged();
	
	UFUNCTION()
	void OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

	UFUNCTION(Server, Reliable)
	void ServerRelayAbilityChainTransition(FActAbilityChainReplicatedTransition Transition);

private:
	
	TUniquePtr<FActBattleInputAnalyzer> BattleInputAnalyzer;
	TUniquePtr<FActBattleCommandResolver> ComboRuntime;
	FActInputCommandMatched InputCommandMatched;
	EInputDirection CurrentAnalyzerDirection = EInputDirection::Neutral;
	TArray<FGameplayTag> PendingAbilityInputPressed;
	TArray<FGameplayTag> PendingAbilityInputReleased;
	TMap<FGameplayTag, double> ActiveAbilityInputTagLastSeenTimes;
};

#undef UE_API
