#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/CharacterMovementReplication.h"

#include "Character/ActCharacterMovementTypes.h"

class UActCharacterMovementComponent;

struct FActPredictedMotionSnapshot
{
	int32 LocalHandle = INDEX_NONE;
	int32 SyncId = INDEX_NONE;
	FActMotionParams Params;
	float ElapsedTime = 0.0f;
	float RotationElapsedTime = 0.0f;
	float FrozenBasisYawDegrees = 0.0f;
	FVector FrozenFacingDirection = FVector::ZeroVector;
	bool bRotationCompleted = false;
	bool bRootMotionRotationSuppressed = false;

	bool HasNetworkIdentity() const
	{
		return SyncId != INDEX_NONE;
	}
};

struct FActCharacterNetworkMoveData : public FCharacterNetworkMoveData
{
	using Super = FCharacterNetworkMoveData;

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	TArray<FActPredictedMotionSnapshot, TInlineAllocator<4>> MotionSnapshots;
};

struct FActCharacterNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
	FActCharacterNetworkMoveDataContainer();

private:
	FActCharacterNetworkMoveData MoveData[3];
};

class FActSavedMove_Character : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	uint32 MotionStateRevision = 0;
	TArray<FActPredictedMotionSnapshot, TInlineAllocator<4>> MotionSnapshots;
};

class FActNetworkPredictionData_Client_Character : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FActNetworkPredictionData_Client_Character(const UCharacterMovementComponent& ClientMovement);
	virtual FSavedMovePtr AllocateNewMove() override;
};
