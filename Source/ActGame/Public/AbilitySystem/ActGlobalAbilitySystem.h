// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayAbilitySpecHandle.h"
#include "Templates/SubclassOf.h"

#include "ActGlobalAbilitySystem.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UActAbilitySystemComponent;
class UObject;
struct FActiveGameplayEffectHandle;
struct FFrame;
struct FGameplayAbilitySpecHandle;

USTRUCT()
struct FGlobalAppliedAbilityList
{
	GENERATED_BODY()
	
	UPROPERTY()
	TMap<TObjectPtr<UActAbilitySystemComponent>, FGameplayAbilitySpecHandle> Handles;
	
	void AddToASC(TSubclassOf<UGameplayAbility> Ability, UActAbilitySystemComponent* ASC);
	void RemoveFromASC(UActAbilitySystemComponent* ASC);
	void RemoveFromAll();
};

USTRUCT()
struct FGlobalAppliedEffectList
{
	GENERATED_BODY()
	
	UPROPERTY()
	TMap<TObjectPtr<UActAbilitySystemComponent>, FActiveGameplayEffectHandle> Handles;
	
	void AddToASC(TSubclassOf<UGameplayEffect> Effect, UActAbilitySystemComponent* ASC);
	void RemoveFromASC(UActAbilitySystemComponent* ASC);
	void RemoveFromAll();
};

UCLASS()
class ACTGAME_API UActGlobalAbilitySystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	
	UActGlobalAbilitySystem();
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Act")
	void ApplyAbilityToAll(TSubclassOf<UGameplayAbility> Ability);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Act")
	void ApplyEffectToAll(TSubclassOf<UGameplayEffect> Effect);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Act")
	void RemoveAbilityFromAll(TSubclassOf<UGameplayAbility> Ability);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Act")
	void RemoveEffectFromAll(TSubclassOf<UGameplayEffect> Effect);
	
	/** Register an ASC with global system and apply any active global effects/abilities. */
	void RegisterASC(UActAbilitySystemComponent* ASC);

	/** Removes an ASC from the global system, along with any active global effects/abilities. */
	void UnregisterASC(UActAbilitySystemComponent* ASC);
	
private:
	
	UPROPERTY()
	TMap<TSubclassOf<UGameplayAbility>, FGlobalAppliedAbilityList> AppliedAbilities;

	UPROPERTY()
	TMap<TSubclassOf<UGameplayEffect>, FGlobalAppliedEffectList> AppliedEffects;

	UPROPERTY()
	TArray<TObjectPtr<UActAbilitySystemComponent>> RegisteredASCs;
	
};
