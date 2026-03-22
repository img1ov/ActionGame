// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ModularPlayerState.h"
#include "AbilitySystemInterface.h"
#include "Character/ActPawnData.h"
#include "Teams/ActTeamAgentInterface.h"

#include "ActPlayerState.generated.h"

#define UE_API ACTGAME_API

class UActExperienceDefinition;
class AController;
class AActPlayerController;
class APlayerState;
class UAbilitySystemComponent;
class UActAbilitySystemComponent;
class UActPawnData;

class UObject;
struct FFrame;
struct FGameplayTag;

/**
 * AActPlayerState
 *
 *	Base player state class used by this project.
 */
UCLASS(MinimalAPI, Config = Game)
class AActPlayerState : public AModularPlayerState, public IAbilitySystemInterface, public IActTeamAgentInterface
{
	GENERATED_BODY()

public:
	
	AActPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Act|PlayerState")
	AActPlayerController* GetActPlayerController() const;

	UFUNCTION(BlueprintCallable, Category = "Act|PlayerState")
	UActAbilitySystemComponent* GetActAbilitySystemComponent() const { return AbilitySystemComponent; }
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	template<class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	void SetPawnData(const UActPawnData* InPawnData);
	
	//~AActor interface
	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;
	//~End of AActor interface
	
	//~IActTeamAgentInterface interface
	UE_API virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	UE_API virtual FGenericTeamId GetGenericTeamId() const override;
	UE_API virtual FOnActTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	//~End of IActTeamAgentInterface interface

protected:
	
	UFUNCTION()
	void OnRep_PawnData();

private:
	
	void OnExperienceLoaded(const UActExperienceDefinition* CurrentExperience);

protected:
	
	UPROPERTY(ReplicatedUsing = OnRep_PawnData)
	TObjectPtr<const UActPawnData> PawnData;

private:

	// The ability system component sub-object used by player characters.
	UPROPERTY(VisibleAnywhere, Category = "Act|PlayerState")
	TObjectPtr<UActAbilitySystemComponent> AbilitySystemComponent;

	// Health attribute set used by this actor.
	UPROPERTY()
	TObjectPtr<const class UActHealthSet> HealthSet;

	// Combat attribute set used by this actor.
	UPROPERTY()
	TObjectPtr<const class UActCombatSet> CombatSet;
	
	UPROPERTY(ReplicatedUsing=OnRep_MyTeamID)
	FGenericTeamId MyTeamID;
	
	UPROPERTY()
	FOnActTeamIndexChangedDelegate OnTeamChangedDelegate;
	
private:
	
	UFUNCTION()
	UE_API void OnRep_MyTeamID(FGenericTeamId OldTeamID);
	
};

#undef UE_API
