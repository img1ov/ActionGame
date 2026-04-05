// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "AT_WaitGameplayEvents.generated.h"

class UActAbilitySystemComponent;

/** Delegate type for gameplay events. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRPGWaitGameplayEventsDelegate, FGameplayTag, EventTag, FGameplayEventData, EventData);

/**
 * Wait for gameplay events matching a set of tags.
 * This task only listens to events and forwards them to Blueprint.
 */
UCLASS()
class ACTGAME_API UAT_WaitGameplayEvents : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAT_WaitGameplayEvents(const FObjectInitializer& ObjectInitializer);

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;
	virtual void OnDestroy(bool AbilityEnded) override;

	/** One of the triggering gameplay events happened */
	UPROPERTY(BlueprintAssignable)
	FRPGWaitGameplayEventsDelegate EventReceived;

	/**
	 * Wait for gameplay events matching EventTags. If EventTags is empty, all events will trigger callback.
	 *
	 * @param TaskInstanceName Set to override the name of this task, for later querying
	 * @param EventTags Any gameplay events matching this tag will activate the EventReceived callback. If empty, all events will trigger callback
	 */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAT_WaitGameplayEvents* WaitGameplayEvents(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		FGameplayTagContainer EventTags);

private:
	/** List of tags to match against gameplay events */
	UPROPERTY()
	FGameplayTagContainer EventTags;

	/** Returns our ability system component */
	UActAbilitySystemComponent* GetTargetASC();

	void OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	FDelegateHandle EventHandle;
};
