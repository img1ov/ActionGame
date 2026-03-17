// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

namespace ActGameplayTags
{
	ACTGAME_API FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString = false);

	// Declare all the custom native tags that Act will use
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_IsDead);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_ActivationGroup);
	
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Behavior_SurvivesDeath);
	
	ACTGAME_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	ACTGAME_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Mouse);
	
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_Spawned);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataAvailable);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataInitialized);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_GameplayReady);
	
	ACTGAME_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Death);
	ACTGAME_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Reset);
	ACTGAME_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_RequestReset);
	
	ACTGAME_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	ACTGAME_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Heal);
	
	ACTGAME_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cheat_GodMode);
	ACTGAME_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cheat_UnlimitedHealth);

	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Crouching);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dying);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dead);
	
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Walking);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_NavWalking);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Falling);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Swimming);
	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Flying);

	ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Custom);
	
	// These are mappings from MovementMode enums to GameplayTags associated with those enums (below)
	ACTGAME_API extern const TMap<uint8, FGameplayTag> MovementModeTagMap;
	ACTGAME_API extern const TMap<uint8, FGameplayTag> CustomMovementModeTagMap;
}
