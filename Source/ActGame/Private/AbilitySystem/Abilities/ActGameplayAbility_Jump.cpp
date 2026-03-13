// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/ActGameplayAbility_Jump.h"

#include "Character/ActCharacter.h"

UActGameplayAbility_Jump::UActGameplayAbility_Jump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UActGameplayAbility_Jump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	const AActCharacter* ActCharacter = Cast<AActCharacter>(ActorInfo->AvatarActor.Get());
	if (!ActCharacter || !ActCharacter->CanJump())
	{
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags))
	{
		return false;
	}

	return true;
}

void UActGameplayAbility_Jump::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Stop jumping in case the ability blueprint doesn't call it.
	CharacterJumpStop();
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UActGameplayAbility_Jump::CharacterJumpStart()
{
	if (AActCharacter* ActCharacter = GetActCharacterFromActorInfo())
	{
		if (ActCharacter->IsLocallyControlled() && !ActCharacter->bPressedJump)
		{
			ActCharacter->UnCrouch();
			ActCharacter->Jump();
		}
	}
}

void UActGameplayAbility_Jump::CharacterJumpStop()
{
	if (AActCharacter* ActCharacter = GetActCharacterFromActorInfo())
	{
		if (ActCharacter->IsLocallyControlled() && !ActCharacter->bPressedJump)
		{
			ActCharacter->StopJumping();
		}
	}
}
