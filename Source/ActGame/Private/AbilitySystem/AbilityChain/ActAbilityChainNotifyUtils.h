#pragma once

#include "AbilitySystemInterface.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

namespace ActAbilityChainNotifyUtils
{
	inline UActAbilitySystemComponent* ResolveActASC(const USkeletalMeshComponent* MeshComp)
	{
		const APawn* Pawn = MeshComp ? Cast<APawn>(MeshComp->GetOwner()) : nullptr;
		if (!Pawn)
		{
			return nullptr;
		}

		if (const IAbilitySystemInterface* AbilitySystemPawn = Cast<IAbilitySystemInterface>(Pawn))
		{
			return Cast<UActAbilitySystemComponent>(AbilitySystemPawn->GetAbilitySystemComponent());
		}

		if (const AController* Controller = Pawn->GetController())
		{
			if (const IAbilitySystemInterface* AbilitySystemController = Cast<IAbilitySystemInterface>(Controller))
			{
				return Cast<UActAbilitySystemComponent>(AbilitySystemController->GetAbilitySystemComponent());
			}
		}

		if (const APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			if (const IAbilitySystemInterface* AbilitySystemPlayerState = Cast<IAbilitySystemInterface>(PlayerState))
			{
				return Cast<UActAbilitySystemComponent>(AbilitySystemPlayerState->GetAbilitySystemComponent());
			}
		}

		return nullptr;
	}
}
