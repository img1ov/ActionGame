// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ActHealthComponent.h"

#include "ActLogChannels.h"

UActHealthComponent::UActHealthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UActHealthComponent::InitializeWithAbilitySystem(UActAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);
	
	if (AbilitySystemComponent)
	{
		UE_LOG(LogAct, Error, TEXT("UActHealthComponent: Health component for owner [%s] has already been initialized with an ability system."), *GetNameSafe(Owner));
		return;
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogAct, Error, TEXT("UActHealthComponent: Cannot initialize health component for owner [%s] with NULL ability system."), *GetNameSafe(Owner));
		return;
	}
}

void UActHealthComponent::UninitializeFromAbilitySystem()
{
	AbilitySystemComponent = nullptr;
}
