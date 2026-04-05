#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Character/ActCharacterMovementTypes.h"
#include "Character/ActCharacterMovementNetworking.h"
#include "ActCharacterMovementComponent.generated.h"

class AActor;

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

	/** Enable/disable lock-on strafe behavior (rotation faces target while movement follows controller). */
	void SetLockOnStrafeActive(bool bActive);

	/** Returns whether lock-on strafe is currently enabled. */
	bool IsLockOnStrafeActive() const { return bLockOnStrafeActive; }

	/** Constant add-move (world-space), lasts until overwritten or cleared. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	void SetAddMove(const FVector& MoveVelocity, bool bIgnoreGravity = false);

	/** Constant add-move (world-space) for a duration. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	void SetAddMoveConstantForce(const FVector& MoveVelocity, float Duration, bool bIgnoreGravity = false);

	/** Add-move rotation driven by a world-space direction. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	void SetAddRotation(const FVector& Direction, float InRotationRate = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Floating")
	void StartFloating(float Duration = -1.0f);
	
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Floating")
	void StopFloating();

public:
	UPROPERTY(BlueprintAssignable, Category = "Act|CharacterMovement")
	FOnAccelerationStateChangedSignature OnAccelerationStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Act|CharacterMovement")
	FOnGroundStateChangedSignature OnGroundStateChanged;

protected:
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual bool CanAttemptJump() const override;
	virtual void PhysicsRotation(float DeltaSeconds) override;

protected:
	FActCharacterGroundInfo CachedGroundInfo;

private:
	struct FActMovementStateStackEntry
	{
		int32 Handle = INDEX_NONE;
		FActMovementStateParams Params;
	};

	void RefreshMovementStateParamsFromStack();

	/** Resolve the lock-on target from the battle component if present. */
	AActor* ResolveLockOnStrafeTarget() const;

	/** Compute the desired facing rotation for lock-on strafe. */
	FRotator GetLockOnStrafeDesiredRotation(const AActor* Target) const;

	/** Apply lock-on strafe rotation when movement input is active. */
	void ApplyLockOnStrafeRotation(float DeltaSeconds);
	void ApplyAddMove(const float DeltaSeconds);
	void ResetAddMoveState();
	void ResetAddRotationState();
	bool TryApplyAddRotation(float DeltaSeconds);
	void UpdateGravityOverride();
	void ApplyNetworkMoveData(const FActAddMoveNetworkState& InAddMoveState, const FActAddRotationNetworkState& InAddRotationState);

private:
	bool bHasAcceleration = false;
	bool bIsOnGround = false;
	bool bLockOnStrafeActive = false;
	int32 NextMovementStateHandle = 1;

	FActMovementStateParams DefaultMovementStateParams;
	TArray<FActMovementStateStackEntry> MovementStateStack;

	/** Active add-move translation state. */
	FActAddMoveNetworkState AddMoveState;
	/** Active add-move rotation state. */
	FActAddRotationNetworkState AddRotationState;

	/** Stop rotating when remaining yaw is within this tolerance (degrees). */
	UPROPERTY(EditAnywhere, Category = "Act|AddMove")
	float AddRotationStopTolerance = 2.0f;

	float SavedGravityScale = 1.0f;
	bool bGravityOverrideActive = false;

	uint32 AddMoveStateRevision = 0;

	mutable FActCharacterNetworkMoveDataContainer NetworkMoveDataContainer;
	
	bool bIsFloating;
    float FloatingDuration;
    FTimerHandle FloatingTimerHandle;
	
	void EndFloating();
	
	friend class FActSavedMove_Character;
	friend struct FActCharacterNetworkMoveData;
};
