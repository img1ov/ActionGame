// Fill out your copyright notice in the Description page of Project Settings.


#include "System/ActGameInstance.h"

#include "ActGameplayTags.h"
#include "Components/GameFrameworkComponentManager.h"

UActGameInstance::UActGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UActGameInstance::Init()
{
	Super::Init();

	// Register our custom init states
	UGameFrameworkComponentManager* ComponentManager = GetSubsystem<UGameFrameworkComponentManager>(this);

	if (ensure(ComponentManager))
	{
		ComponentManager->RegisterInitState(ActGameplayTags::InitState_Spawned, false, FGameplayTag());
		ComponentManager->RegisterInitState(ActGameplayTags::InitState_DataAvailable, false, ActGameplayTags::InitState_Spawned);
		ComponentManager->RegisterInitState(ActGameplayTags::InitState_DataInitialized, false, ActGameplayTags::InitState_DataAvailable);
		ComponentManager->RegisterInitState(ActGameplayTags::InitState_GameplayReady, false, ActGameplayTags::InitState_DataInitialized);
	}

	//TODO: Initialize the debug key with a set value for AES256. This is not secure and for example purposes only.
}
