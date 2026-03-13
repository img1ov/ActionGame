// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ActCharacterMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAccelerationStateChangedSignature, bool, bOldHasAcceleration, bool, bNewHasAcceleration);

USTRUCT(BlueprintType)
struct FActCharacterGroundInfo
{
	GENERATED_BODY()

	FActCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{}

	uint64 LastUpdateFrame;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};

/**
 * 
 */
UCLASS()
class ACTGAME_API UActCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	
	UActCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	void SetReplicatedAcceleration(const FVector& InAcceleration);
	
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement")
	const FActCharacterGroundInfo& GetGroundInfo();
	
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "HasAcceleration"), Category = "Act|CharacterMovement")
	FORCEINLINE bool GetHasAcceleration () const{ return GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER; }

protected:
	
	/** ~UCharacterMovementComponent Interface */
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void SimulateMovement(float DeltaTime) override;
	virtual bool CanAttemptJump() const override;
	/** ~End UCharacterMovementComponent Interface */

	
public:
	
	UPROPERTY(BlueprintAssignable, Category="Act|CharacterMovement")
	FOnAccelerationStateChangedSignature OnAccelerationStateChanged;
	
protected:
	
	FActCharacterGroundInfo CachedGroundInfo;

	UPROPERTY(Transient)
	bool bHasReplicatedAcceleration = false;
	
	bool bHasAcceleration = false;
};
