#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Character/ActCharacterMovementTypes.h"
#include "ActCharacterMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAccelerationStateChangedSignature, bool, bOldHasAcceleration, bool, bNewHasAcceleration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGroundStateChangedSignature, bool, bOldIsOnGround, bool, bNewIsOnGround);

USTRUCT(BlueprintType)
struct FActCharacterGroundInfo
{
	GENERATED_BODY()

	FActCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{
	}

	uint64 LastUpdateFrame;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};

UCLASS()
class ACTGAME_API UActCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UActCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	static UActCharacterMovementComponent* ResolveActMovementComponent(const AActor* AvatarActor);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement")
	const FActCharacterGroundInfo& GetGroundInfo();

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "HasAcceleration"), Category = "Act|CharacterMovement")
	bool GetHasAcceleration() const
	{
		return GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER;
	}

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	void ApplyMovementStateParams(const FActMovementStateParams& Params);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	void ResetMovementStateParams();

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	int32 PushMovementStateParams(const FActMovementStateParams& Params, int32 ExistingHandle = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	bool PopMovementStateParams(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	void ClearMovementStateParamsStack();

public:
	UPROPERTY(BlueprintAssignable, Category = "Act|CharacterMovement")
	FOnAccelerationStateChangedSignature OnAccelerationStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Act|CharacterMovement")
	FOnGroundStateChangedSignature OnGroundStateChanged;

protected:
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual bool CanAttemptJump() const override;

protected:
	FActCharacterGroundInfo CachedGroundInfo;

private:
	struct FActMovementStateStackEntry
	{
		int32 Handle = INDEX_NONE;
		FActMovementStateParams Params;
	};

	void RefreshMovementStateParamsFromStack();

private:
	bool bHasAcceleration = false;
	bool bIsOnGround = false;
	int32 NextMovementStateHandle = 1;

	FActMovementStateParams DefaultMovementStateParams;
	TArray<FActMovementStateStackEntry> MovementStateStack;
};
