// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/GameFrameworkComponent.h"

#include "ActHealthComponent.generated.h"

class UActAbilitySystemComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActhealth_DeathEvent, AActor*, OwningActor);

/**
 * UActHealthComponent
 *
 *	An actor component used to handle anything related to health.
 */
UCLASS(Blueprintable, Meta=(BlueprintSpawnableComponent))
class ACTGAME_API UActHealthComponent : public UGameFrameworkComponent
{
	GENERATED_BODY()

public:
	
	UActHealthComponent(const FObjectInitializer& ObjectInitializer);

	// Initialize the component using an ability system component.
	UFUNCTION(BlueprintCallable, Category = "Act|Health")
	void InitializeWithAbilitySystem(UActAbilitySystemComponent* InASC);

	UFUNCTION(BlueprintCallable, Category = "Act|Health")
	void UninitializeFromAbilitySystem();

public:
	
	// Delegate fired when the death sequence has started.
	UPROPERTY(BlueprintAssignable)
	FActhealth_DeathEvent OnDeathStarted;

	// Delegate fired when the death sequence has finished.
	UPROPERTY(BlueprintAssignable)
	FActhealth_DeathEvent OnDeathFinished;

protected:

	// Ability system used by this component.
	UPROPERTY()
	TObjectPtr<UActAbilitySystemComponent> AbilitySystemComponent;
};
