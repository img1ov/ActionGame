#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/CharacterMovementReplication.h"

#include "Character/ActCharacterMovementTypes.h"

class UActCharacterMovementComponent;

/**
 * A single active AddMove state captured for movement prediction/replication.
 *
 * Notes:
 * - LocalHandle is only meaningful on the local machine and is not serialized for networking.
 * - SyncId provides a stable identity across client prediction, move combining, and server simulation.
 */
struct FActAddMoveSnapshot
{
	/** Local handle in UActCharacterMovementComponent::VelocityAdditionMap (not network stable). */
	int32 LocalHandle = INDEX_NONE;

	/** Stable identity used for network alignment (must be non-INDEX_NONE to replicate). */
	int32 SyncId = INDEX_NONE;

	/** Parameters describing how this AddMove was authored. */
	FActAddMoveParams Params;

	/** Elapsed time (seconds) inside the authored duration. */
	float ElapsedTime = 0.0f;

	/** True if this snapshot should participate in network synchronization. */
	bool HasNetworkIdentity() const
	{
		return SyncId != INDEX_NONE;
	}
};

/**
 * Per-move custom payload sent through CharacterMovement RPCs.
 *
 * We serialize active AddMove snapshots so the server can simulate the same
 * motion that the client predicted (reduces correction jitter under bad network).
 */
struct FActCharacterNetworkMoveData : public FCharacterNetworkMoveData
{
	using Super = FCharacterNetworkMoveData;

	/** Copies data from the FSavedMove (client-side) into this move payload. */
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;

	/** Serializes AddMove snapshots along with the default Character movement fields. */
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	/** Active predicted AddMove states for this movement frame. */
	TArray<FActAddMoveSnapshot, TInlineAllocator<4>> AddMoveSnapshots;
};

/**
 * Container used by UCharacterMovementComponent to store New/Pending/Old move payloads.
 * We provide 3 instances to match UE's base container layout.
 */
struct FActCharacterNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
	FActCharacterNetworkMoveDataContainer();

private:
	FActCharacterNetworkMoveData ActMoveData[3];
};

/**
 * Client-side saved move extension.
 *
 * We store AddMove snapshots and a revision number used to gate move combining:
 * if the AddMove state changes (e.g. montage section switch), we prevent combining
 * so the server receives the correct motion profile boundaries.
 */
class FActSavedMove_Character : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	/** Resets all state to default. */
	virtual void Clear() override;

	/** Captures AddMove snapshots from the movement component for this move. */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;

	/** Replays AddMove snapshots back into the movement component before simulation. */
	virtual void PrepMoveFor(ACharacter* C) override;

	/** Disallows combining when AddMove state revision differs. */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	/** Revision gate so section changes / motion profile swaps do not combine into stale moves. */
	uint32 AddMoveStateRevision = 0;
	TArray<FActAddMoveSnapshot, TInlineAllocator<4>> AddMoveSnapshots;
};

/**
 * Client prediction allocator that produces our extended saved move type.
 */
class FActNetworkPredictionData_Client_Character : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FActNetworkPredictionData_Client_Character(const UCharacterMovementComponent& ClientMovement);
	virtual FSavedMovePtr AllocateNewMove() override;
};
