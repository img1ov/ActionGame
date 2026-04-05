#include "Character/ActCharacterMovementNetworking.h"

#include "Character/ActCharacterMovementComponent.h"
#include "GameFramework/Character.h"

void FActCharacterNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	if (const FActSavedMove_Character* ActMove = static_cast<const FActSavedMove_Character*>(&ClientMove))
	{
		AddMoveState = ActMove->AddMoveState;
		AddRotationState = ActMove->AddRotationState;
	}
	else
	{
		AddMoveState = FActAddMoveNetworkState();
		AddRotationState = FActAddRotationNetworkState();
	}
}

bool FActCharacterNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	if (!Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType))
	{
		return false;
	}

	Ar.SerializeBits(&AddMoveState.bActive, 1);
	if (AddMoveState.bActive)
	{
		Ar << AddMoveState.Velocity;
		Ar << AddMoveState.Duration;
		Ar << AddMoveState.ElapsedTime;
		Ar.SerializeBits(&AddMoveState.bIgnoreGravity, 1);
	}
	else if (Ar.IsLoading())
	{
		AddMoveState = FActAddMoveNetworkState();
	}

	Ar.SerializeBits(&AddRotationState.bActive, 1);
	if (AddRotationState.bActive)
	{
		Ar.SerializeBits(&AddRotationState.bUseDirection, 1);
		if (AddRotationState.bUseDirection)
		{
			Ar << AddRotationState.Direction;
		}
		else
		{
			Ar << AddRotationState.Rotation;
		}
		Ar << AddRotationState.RotationRate;
	}
	else if (Ar.IsLoading())
	{
		AddRotationState = FActAddRotationNetworkState();
	}

	return !Ar.IsError();
}

FActCharacterNetworkMoveDataContainer::FActCharacterNetworkMoveDataContainer()
{
	NewMoveData = &ActMoveData[0];
	PendingMoveData = &ActMoveData[1];
	OldMoveData = &ActMoveData[2];
}

void FActSavedMove_Character::Clear()
{
	Super::Clear();
	AddMoveStateRevision = 0;
	AddMoveState = FActAddMoveNetworkState();
	AddRotationState = FActAddRotationNetworkState();
}

void FActSavedMove_Character::SetMoveFor(ACharacter* C, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UActCharacterMovementComponent* ActMovement = C ? Cast<UActCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		AddMoveStateRevision = ActMovement->AddMoveStateRevision;
		AddMoveState = ActMovement->AddMoveState;
		AddRotationState = ActMovement->AddRotationState;
	}
}

void FActSavedMove_Character::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UActCharacterMovementComponent* ActMovement = C ? Cast<UActCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		ActMovement->ApplyNetworkMoveData(AddMoveState, AddRotationState);
	}
}

bool FActSavedMove_Character::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	if (!Super::CanCombineWith(NewMove, InCharacter, MaxDelta))
	{
		return false;
	}

	const FActSavedMove_Character* ActNewMove = static_cast<const FActSavedMove_Character*>(NewMove.Get());
	return ActNewMove && AddMoveStateRevision == ActNewMove->AddMoveStateRevision;
}

FActNetworkPredictionData_Client_Character::FActNetworkPredictionData_Client_Character(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FActNetworkPredictionData_Client_Character::AllocateNewMove()
{
	return FSavedMovePtr(new FActSavedMove_Character());
}
