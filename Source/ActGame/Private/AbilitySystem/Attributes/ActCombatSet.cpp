// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Attributes/ActCombatSet.h"

#include "Net/UnrealNetwork.h"

UActCombatSet::UActCombatSet()
	: BaseDamage(0.0f)
	, BaseHeal(0.0f)
{
}

void UActCombatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UActCombatSet, BaseDamage, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActCombatSet, BaseHeal, COND_OwnerOnly, REPNOTIFY_Always);
}

void UActCombatSet::OnRep_BaseDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActCombatSet, BaseDamage, OldValue);
}

void UActCombatSet::OnRep_BaseHeal(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActCombatSet, BaseHeal, OldValue);
}
