// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/ActAbilitySystemGlobals.h"

#include "AbilitySystem/ActGameplayEffectContext.h"

struct FGameplayEffectContext;

UActAbilitySystemGlobals::UActAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FGameplayEffectContext* UActAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FActGameplayEffectContext();
}
