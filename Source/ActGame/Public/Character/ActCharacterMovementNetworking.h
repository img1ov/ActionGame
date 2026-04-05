#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/CharacterMovementReplication.h"

struct FActAddMoveNetworkState
{
	bool bActive = false;
	FVector Velocity = FVector::ZeroVector;
	float Duration = 0.0f;
	float ElapsedTime = 0.0f;
	bool bIgnoreGravity = false;
};

struct FActAddRotationNetworkState
{
	bool bActive = false;
	bool bUseDirection = false;
	FVector Direction = FVector::ForwardVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float RotationRate = -1.0f;
};

struct FActCharacterNetworkMoveData : public FCharacterNetworkMoveData
{
	using Super = FCharacterNetworkMoveData;

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	FActAddMoveNetworkState AddMoveState;
	FActAddRotationNetworkState AddRotationState;
};

struct FActCharacterNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
	FActCharacterNetworkMoveDataContainer();

private:
	FActCharacterNetworkMoveData ActMoveData[3];
};

class FActSavedMove_Character : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	uint32 AddMoveStateRevision = 0;
	FActAddMoveNetworkState AddMoveState;
	FActAddRotationNetworkState AddRotationState;
};

class FActNetworkPredictionData_Client_Character : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FActNetworkPredictionData_Client_Character(const UCharacterMovementComponent& ClientMovement);
	virtual FSavedMovePtr AllocateNewMove() override;
};
