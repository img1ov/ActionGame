// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Attributes/ActAttributeSet.h"

#include "AbilitySystem/ActAbilitySystemComponent.h"

UActAttributeSet::UActAttributeSet()
{
}

UWorld* UActAttributeSet::GetWorld() const
{
	const UObject* Outer = GetOuter();
	check(Outer);

	return Outer->GetWorld();
}

UActAbilitySystemComponent* UActAttributeSet::GetActAbilitySystemComponent() const
{
	return Cast<UActAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}
