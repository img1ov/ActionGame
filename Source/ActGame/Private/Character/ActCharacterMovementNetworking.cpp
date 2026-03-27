#include "Character/ActCharacterMovementNetworking.h"

#include "Animation/AnimMontage.h"
#include "Character/ActCharacterMovementComponent.h"
#include "GameFramework/Character.h"

namespace
{
template <typename TObjectClass>
bool SerializeObjectReference(FArchive& Ar, UPackageMap* PackageMap, TObjectPtr<TObjectClass>& ObjectReference)
{
	UObject* RawObject = ObjectReference.Get();
	const bool bSuccess = PackageMap ? PackageMap->SerializeObject(Ar, TObjectClass::StaticClass(), RawObject) : false;
	if (Ar.IsLoading())
	{
		ObjectReference = Cast<TObjectClass>(RawObject);
	}

	return bSuccess;
}

bool SerializeAddMoveSnapshot(FArchive& Ar, UPackageMap* PackageMap, FActAddMoveSnapshot& Snapshot)
{
	// We serialize only the fields required for server to reconstruct the same predicted AddMove state.
	// LocalHandle is intentionally excluded (not stable across machines).
	Ar << Snapshot.SyncId;
	Ar << Snapshot.ElapsedTime;
	Ar << Snapshot.Params.SourceType;
	Ar << Snapshot.Params.Space;
	Ar << Snapshot.Params.Velocity;
	Ar << Snapshot.Params.Duration;
	Ar << Snapshot.Params.CurveType;

	if (!SerializeObjectReference(Ar, PackageMap, Snapshot.Params.Curve))
	{
		return false;
	}

	if (!SerializeObjectReference(Ar, PackageMap, Snapshot.Params.Montage))
	{
		return false;
	}

	Ar << Snapshot.Params.StartTrackPosition;
	Ar << Snapshot.Params.EndTrackPosition;
	Ar << Snapshot.Params.bApplyRotation;

	// Params.SyncId is transient on the params struct; we mirror the serialized identity here to keep equality checks consistent.
	Snapshot.Params.SyncId = Snapshot.SyncId;
	return true;
}
}

void FActCharacterNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	AddMoveSnapshots.Reset();
	if (const FActSavedMove_Character* ActMove = static_cast<const FActSavedMove_Character*>(&ClientMove))
	{
		// Only replicate snapshots that have SyncId.
		// Local-only AddMove entries are still replayed client-side but do not affect server simulation.
		for (const FActAddMoveSnapshot& Snapshot : ActMove->AddMoveSnapshots)
		{
			if (Snapshot.HasNetworkIdentity())
			{
				AddMoveSnapshots.Add(Snapshot);
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

	uint8 SnapshotCount = static_cast<uint8>(FMath::Min(AddMoveSnapshots.Num(), 255));
	Ar << SnapshotCount;

	if (Ar.IsLoading())
	{
		AddMoveSnapshots.Reset();
		AddMoveSnapshots.Reserve(SnapshotCount);
	}

	for (uint8 SnapshotIndex = 0; SnapshotIndex < SnapshotCount; ++SnapshotIndex)
	{
		FActAddMoveSnapshot Snapshot;
		if (!Ar.IsLoading())
		{
			Snapshot = AddMoveSnapshots[SnapshotIndex];
		}

		if (!SerializeAddMoveSnapshot(Ar, PackageMap, Snapshot))
		{
			return false;
		}

		if (Ar.IsLoading())
		{
			AddMoveSnapshots.Add(Snapshot);
		}
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
	AddMoveSnapshots.Reset();
}

void FActSavedMove_Character::SetMoveFor(ACharacter* C, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UActCharacterMovementComponent* ActMovement = C ? Cast<UActCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		AddMoveStateRevision = ActMovement->GetAddMoveStateRevision();
		ActMovement->CaptureAddMoveSnapshots(AddMoveSnapshots);
	}
}

void FActSavedMove_Character::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UActCharacterMovementComponent* ActMovement = C ? Cast<UActCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		ActMovement->RestoreAddMoveSnapshots(AddMoveSnapshots, false);
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
