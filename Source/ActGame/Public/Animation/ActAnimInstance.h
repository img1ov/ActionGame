// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayEffectTypes.h"
#include "Animation/AnimInstance.h"
#include "ActAnimInstance.generated.h"

class UActCharacterMovementComponent;
class UAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class ACTGAME_API UActAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

protected:
	virtual void NativeInitializeAnimation() override;
	
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	UPROPERTY(BlueprintReadOnly, Category = "CharacterStateData")
	float GroundDistance = -1.0f;
};
