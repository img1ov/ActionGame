// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/Tasks/AT_WaitGameplayEvents.h"

#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"

UAT_WaitGameplayEvents::UAT_WaitGameplayEvents(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UActAbilitySystemComponent* UAT_WaitGameplayEvents::GetTargetASC()
{
	return Cast<UActAbilitySystemComponent>(AbilitySystemComponent);
}

UAT_WaitGameplayEvents* UAT_WaitGameplayEvents::WaitGameplayEvents(UGameplayAbility* OwningAbility, FName TaskInstanceName, FGameplayTagContainer InEventTags)
{
	UAT_WaitGameplayEvents* MyObj = NewAbilityTask<UAT_WaitGameplayEvents>(OwningAbility, TaskInstanceName);
	MyObj->EventTags = InEventTags;
	return MyObj;
}

void UAT_WaitGameplayEvents::Activate()
{
	if (!Ability)
	{
		return;
	}

	UActAbilitySystemComponent* ActAbilitySystemComponent = GetTargetASC();
	if (ActAbilitySystemComponent)
	{
		EventHandle = ActAbilitySystemComponent->AddGameplayEventTagContainerDelegate(
			EventTags,
			FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UAT_WaitGameplayEvents::OnGameplayEvent));
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAT_WaitGameplayEvents called on invalid AbilitySystemComponent"));
	}

	SetWaitingOnAvatar();
}

void UAT_WaitGameplayEvents::ExternalCancel()
{
	Super::ExternalCancel();
}

void UAT_WaitGameplayEvents::OnDestroy(bool AbilityEnded)
{
	UActAbilitySystemComponent* ActAbilitySystemComponent = GetTargetASC();
	if (ActAbilitySystemComponent)
	{
		ActAbilitySystemComponent->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

void UAT_WaitGameplayEvents::OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (!ShouldBroadcastAbilityTaskDelegates())
	{
		return;
	}

	FGameplayEventData TempData = Payload ? *Payload : FGameplayEventData();
	TempData.EventTag = EventTag;

	EventReceived.Broadcast(EventTag, TempData);
}

FString UAT_WaitGameplayEvents::GetDebugString() const
{
	return FString::Printf(TEXT("WaitGameplayEvents. EventTags: %s"), *EventTags.ToString());
}
