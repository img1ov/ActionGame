#include "Character/ActCharacterMovementNetworking.h"

#include "Character/ActCharacterMovementComponent.h"
#include "GameFramework/Character.h"

namespace
{
bool SerializeMotionSnapshot(FArchive& Ar, UPackageMap* PackageMap, FActPredictedMotionSnapshot& Snapshot)
{
	(void)PackageMap;
	Ar << Snapshot.SyncId;
	Ar << Snapshot.ElapsedTime;
	Ar << Snapshot.RotationElapsedTime;
	Ar << Snapshot.FrozenBasisYawDegrees;
	Ar << Snapshot.FrozenFacingDirection;
	Ar.SerializeBits(&Snapshot.bRotationCompleted, 1);
	Ar.SerializeBits(&Snapshot.bRootMotionRotationSuppressed, 1);
	Snapshot.Params.SyncId = Snapshot.SyncId;
	return true;
}
}

void FActCharacterNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	MotionSnapshots.Reset();
	if (const FActSavedMove_Character* ActMove = static_cast<const FActSavedMove_Character*>(&ClientMove))
	{
		for (const FActPredictedMotionSnapshot& Snapshot : ActMove->MotionSnapshots)
		{
			if (Snapshot.HasNetworkIdentity())
			{
				MotionSnapshots.Add(Snapshot);
			}
		}
	}
}

bool FActCharacterNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	if (!Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType))
	{
		return false;
	}

	uint8 SnapshotCount = static_cast<uint8>(FMath::Min(MotionSnapshots.Num(), 255));
	Ar << SnapshotCount;

	if (Ar.IsLoading())
	{
		MotionSnapshots.Reset();
		MotionSnapshots.Reserve(SnapshotCount);
	}

	for (uint8 SnapshotIndex = 0; SnapshotIndex < SnapshotCount; ++SnapshotIndex)
	{
		FActPredictedMotionSnapshot Snapshot;
		if (!Ar.IsLoading())
		{
			Snapshot = MotionSnapshots[SnapshotIndex];
		}

		if (!SerializeMotionSnapshot(Ar, PackageMap, Snapshot))
		{
			return false;
		}

		if (Ar.IsLoading())
		{
			MotionSnapshots.Add(Snapshot);
		}
	}

	return !Ar.IsError();
}

FActCharacterNetworkMoveDataContainer::FActCharacterNetworkMoveDataContainer()
{
	NewMoveData = &MoveData[0];
	PendingMoveData = &MoveData[1];
	OldMoveData = &MoveData[2];
}

void FActSavedMove_Character::Clear()
{
	Super::Clear();
	MotionStateRevision = 0;
	MotionSnapshots.Reset();
}

void FActSavedMove_Character::SetMoveFor(ACharacter* C, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UActCharacterMovementComponent* ActMovement = C ? Cast<UActCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		MotionStateRevision = ActMovement->GetMotionStateRevision();
		ActMovement->CapturePredictedMotionSnapshots(MotionSnapshots);
	}
}

void FActSavedMove_Character::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UActCharacterMovementComponent* ActMovement = C ? Cast<UActCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		ActMovement->RestorePredictedMotionSnapshots(MotionSnapshots, false);
	}
}

bool FActSavedMove_Character::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	if (!Super::CanCombineWith(NewMove, InCharacter, MaxDelta))
	{
		return false;
	}

	const FActSavedMove_Character* ActNewMove = static_cast<const FActSavedMove_Character*>(NewMove.Get());
	return ActNewMove && MotionStateRevision == ActNewMove->MotionStateRevision;
}

FActNetworkPredictionData_Client_Character::FActNetworkPredictionData_Client_Character(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FActNetworkPredictionData_Client_Character::AllocateNewMove()
{
	return FSavedMovePtr(new FActSavedMove_Character());
}
