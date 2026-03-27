// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/ActAnimInstance.h"

#include "AbilitySystemGlobals.h"
#include "Character/ActCharacter.h"
#include "Character/ActCharacterMovementComponent.h"

void UActAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	GameplayTagPropertyMap.Initialize(this, ASC);
}

void UActAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (const AActor* OwningActor = GetOwningActor())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
		{
			InitializeWithAbilitySystem(ASC);
		}
	}
}

void UActAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AActCharacter* Character = Cast<AActCharacter>(GetOwningActor());
	if (!Character)
	{
		return;
	}

	UActCharacterMovementComponent* CharMoveComp = CastChecked<UActCharacterMovementComponent>(Character->GetCharacterMovement());
	const FActCharacterGroundInfo& GroundInfo = CharMoveComp->GetGroundInfo();
	GroundDistance = GroundInfo.GroundDistance;
}
