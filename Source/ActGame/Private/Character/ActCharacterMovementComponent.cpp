#include "Character/ActCharacterMovementComponent.h"

#include "ActLogChannels.h"
#include "Animation/AnimMontage.h"
#include "Character/ActBattleComponent.h"
#include "Character/ActCharacter.h"
#include "Character/ActHeroComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagAssetInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Misc/Crc.h"

namespace ActCharacter
{
	static float GroundTraceDistance = 100000.0f;
}

namespace
{
constexpr float DefaultRotationTaskDuration = 0.25f;
constexpr float LockOnRotationTaskDuration = 0.55f;

bool IsReplicatedMotionSource(const FActMotionParams& Params)
{
	return Params.SyncId != INDEX_NONE &&
		Params.Provenance != EActMotionProvenance::ReplicatedExternal &&
		Params.Provenance != EActMotionProvenance::LocalRuntime;
}

bool ShouldAffectPredictionRevision(const FActMotionParams& Params)
{
	return Params.Provenance != EActMotionProvenance::LocalRuntime;
}
}

UActCharacterMovementComponent::UActCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetworkSmoothingMode = ENetworkSmoothingMode::Linear;
	bNetworkAlwaysReplicateTransformUpdateTimestamp = true;
	bCachedIgnoreClientMovementErrorChecksAndCorrection = bIgnoreClientMovementErrorChecksAndCorrection;
	bCachedServerAcceptClientAuthoritativePosition = bServerAcceptClientAuthoritativePosition;
	bIsOnGround = IsMovingOnGround();

	DefaultMovementStateParams.MaxWalkSpeed = MaxWalkSpeed;
	DefaultMovementStateParams.MaxAcceleration = MaxAcceleration;
	DefaultMovementStateParams.BrakingDecelerationWalking = BrakingDecelerationWalking;
	DefaultMovementStateParams.GroundFriction = GroundFriction;
	DefaultMovementStateParams.RotationRate = RotationRate;
	DefaultMovementStateParams.bOrientRotationToMovement = bOrientRotationToMovement;
	DefaultMovementStateParams.bUseControllerDesiredRotation = bUseControllerDesiredRotation;

	SetNetworkMoveDataContainer(NetworkMoveDataContainer);
}

UActCharacterMovementComponent* UActCharacterMovementComponent::ResolveActMovementComponent(const AActor* AvatarActor)
{
	const ACharacter* Character = Cast<ACharacter>(AvatarActor);
	return Character ? Cast<UActCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}

bool UActCharacterMovementComponent::ShouldUseMotionNetworkSync(const UGameplayAbility* Ability, const EActMotionProvenance Provenance)
{
	if (Provenance == EActMotionProvenance::LocalRuntime)
	{
		return false;
	}

	if (!Ability)
	{
		return Provenance == EActMotionProvenance::AuthorityExternal;
	}

	return Provenance == EActMotionProvenance::AuthorityExternal ||
		Ability->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

int32 UActCharacterMovementComponent::BuildStableMotionSyncId(const FName InstanceName, const UGameplayAbility* Ability, const TCHAR* FallbackSeed)
{
	uint32 Hash = GetTypeHash(InstanceName);
	if (Ability)
	{
		Hash = HashCombine(Hash, GetTypeHash(Ability->GetClass()));
		Hash = HashCombine(Hash, FCrc::StrCrc32(*Ability->GetCurrentAbilitySpecHandle().ToString()));
		Hash = HashCombine(Hash, ::GetTypeHash(Ability->GetCurrentActivationInfo().GetActivationPredictionKey().Current));
	}

	int32 SyncId = static_cast<int32>(Hash & 0x7fffffff);
	if (SyncId == INDEX_NONE)
	{
		SyncId = FCrc::StrCrc32(FallbackSeed);
	}

	return SyncId;
}

void UActCharacterMovementComponent::SetReplicatedAcceleration(const FVector& InAcceleration)
{
	bHasReplicatedAcceleration = true;
	Acceleration = InAcceleration;
}

const FActCharacterGroundInfo& UActCharacterMovementComponent::GetGroundInfo()
{
	if (!CharacterOwner || (GFrameCounter == CachedGroundInfo.LastUpdateFrame))
	{
		return CachedGroundInfo;
	}

	if (MovementMode == MOVE_Walking)
	{
		CachedGroundInfo.GroundHitResult = CurrentFloor.HitResult;
		CachedGroundInfo.GroundDistance = 0.0f;
	}
	else
	{
		const UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent();
		check(CapsuleComp);

		const float CapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
		const ECollisionChannel CollisionChannel = UpdatedComponent ? UpdatedComponent->GetCollisionObjectType() : ECC_Pawn;
		const FVector TraceStart(GetActorLocation());
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, TraceStart.Z - ActCharacter::GroundTraceDistance - CapsuleHalfHeight);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ActCharacterMovementComponent_GetGroundInfo), false, CharacterOwner);
		FCollisionResponseParams ResponseParams;
		InitCollisionParams(QueryParams, ResponseParams);

		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, CollisionChannel, QueryParams, ResponseParams);

		CachedGroundInfo.GroundHitResult = HitResult;
		CachedGroundInfo.GroundDistance = ActCharacter::GroundTraceDistance;

		if (MovementMode == MOVE_NavWalking)
		{
			CachedGroundInfo.GroundDistance = 0.0f;
		}
		else if (HitResult.bBlockingHit)
		{
			CachedGroundInfo.GroundDistance = FMath::Max(HitResult.Distance - CapsuleHalfHeight, 0.0f);
		}
	}

	CachedGroundInfo.LastUpdateFrame = GFrameCounter;
	return CachedGroundInfo;
}

void UActCharacterMovementComponent::ApplyMovementStateParams(const FActMovementStateParams& Params)
{
	MaxWalkSpeed = Params.MaxWalkSpeed;
	MaxAcceleration = Params.MaxAcceleration;
	BrakingDecelerationWalking = Params.BrakingDecelerationWalking;
	GroundFriction = Params.GroundFriction;
	RotationRate = Params.RotationRate;
	bOrientRotationToMovement = Params.bOrientRotationToMovement;
	bUseControllerDesiredRotation = Params.bUseControllerDesiredRotation;
}

void UActCharacterMovementComponent::ResetMovementStateParams()
{
	ApplyMovementStateParams(DefaultMovementStateParams);
}

int32 UActCharacterMovementComponent::PushMovementStateParams(const FActMovementStateParams& Params, const int32 ExistingHandle)
{
	if (ExistingHandle != INDEX_NONE)
	{
		if (FActMovementStateStackEntry* ExistingEntry = MovementStateStack.FindByPredicate(
			[ExistingHandle](const FActMovementStateStackEntry& Entry)
			{
				return Entry.Handle == ExistingHandle;
			}))
		{
			ExistingEntry->Params = Params;
			const FActMovementStateStackEntry UpdatedEntry = *ExistingEntry;
			MovementStateStack.RemoveAll([ExistingHandle](const FActMovementStateStackEntry& Entry) { return Entry.Handle == ExistingHandle; });
			MovementStateStack.Add(UpdatedEntry);
			RefreshMovementStateParamsFromStack();
			return ExistingHandle;
		}
	}

	FActMovementStateStackEntry& NewEntry = MovementStateStack.AddDefaulted_GetRef();
	NewEntry.Handle = NextMovementStateHandle++;
	NewEntry.Params = Params;
	RefreshMovementStateParamsFromStack();
	return NewEntry.Handle;
}

bool UActCharacterMovementComponent::PopMovementStateParams(const int32 Handle)
{
	const int32 RemovedCount = MovementStateStack.RemoveAll(
		[Handle](const FActMovementStateStackEntry& Entry)
		{
			return Entry.Handle == Handle;
		});

	if (RemovedCount <= 0)
	{
		return false;
	}

	RefreshMovementStateParamsFromStack();
	return true;
}

void UActCharacterMovementComponent::ClearMovementStateParamsStack()
{
	MovementStateStack.Reset();
	RefreshMovementStateParamsFromStack();
}

int32 UActCharacterMovementComponent::GenerateMotionSyncId()
{
	int32 SyncId = NextGeneratedMotionSyncId++;
	if (SyncId == INDEX_NONE)
	{
		SyncId = NextGeneratedMotionSyncId++;
	}

	return SyncId;
}

int32 UActCharacterMovementComponent::ApplyMotion(const FActMotionParams& Params, USkeletalMeshComponent* Mesh, int32 ExistingHandle)
{
	return ApplyMotionInternal(Params, Mesh, ExistingHandle);
}

int32 UActCharacterMovementComponent::ApplyRotationMotion(
	const FActMotionRotationParams& RotationParams,
	const float Duration,
	const EActMotionModeFilter ModeFilter,
	const EActMotionProvenance Provenance,
	const int32 ExistingHandle,
	const int32 SyncId)
{
	if (!RotationParams.IsValidRequest())
	{
		return INDEX_NONE;
	}

	FActMotionParams Params;
	Params.Duration = Duration > UE_SMALL_NUMBER ? Duration : DefaultRotationTaskDuration;
	Params.ModeFilter = ModeFilter;
	Params.Rotation = RotationParams;
	Params.Provenance = Provenance;
	Params.SyncId = SyncId;
	return ApplyMotionInternal(Params, nullptr, ExistingHandle);
}

int32 UActCharacterMovementComponent::AddMoveRotationToTarget(
	AActor* TargetActor,
	const EActMotionRotationActorMode ActorMode,
	const float Duration,
	const bool bIsAdditive,
	const EActMotionRotationPriority Priority,
	const EActMotionProvenance Provenance)
{
	FActMotionRotationParams RotationParams;
	RotationParams.SourceType = EActMotionRotationSourceType::Actor;
	RotationParams.TargetActor = TargetActor;
	RotationParams.ActorMode = ActorMode;
	RotationParams.bClearOnReached = true;
	RotationParams.bFreezeDirectionAtStart =
		bIsAdditive ||
		ActorMode == EActMotionRotationActorMode::FaceTarget ||
		ActorMode == EActMotionRotationActorMode::BackToTarget;
	RotationParams.bIsAdditive = bIsAdditive;
	RotationParams.Priority = Priority;
	return ApplyRotationMotion(RotationParams, Duration, EActMotionModeFilter::Any, Provenance, INDEX_NONE, INDEX_NONE);
}

int32 UActCharacterMovementComponent::AddMoveRotationToDirection(
	FVector Direction,
	const float Duration,
	const bool bIsAdditive,
	const EActMotionRotationPriority Priority,
	const EActMotionProvenance Provenance)
{
	FActMotionRotationParams RotationParams;
	RotationParams.SourceType = EActMotionRotationSourceType::Direction;
	RotationParams.Direction = Direction.GetSafeNormal();
	RotationParams.bClearOnReached = true;
	RotationParams.bFreezeDirectionAtStart = true;
	RotationParams.bIsAdditive = bIsAdditive;
	RotationParams.Priority = Priority;
	return ApplyRotationMotion(RotationParams, Duration, EActMotionModeFilter::Any, Provenance, INDEX_NONE, INDEX_NONE);
}

int32 UActCharacterMovementComponent::ApplyRootMotionMotion(
	UAnimMontage* Montage,
	float StartTrackPosition,
	float EndTrackPosition,
	float Duration,
	EActMotionBasisMode BasisMode,
	bool bApplyRootMotionRotation,
	bool bRespectHigherPriorityRotation,
	bool bIgnoreZAccumulate,
	FActMotionRotationParams Rotation,
	EActMotionProvenance Provenance,
	int32 ExistingHandle,
	int32 SyncId)
{
	FActMotionParams Params;
	Params.SourceType = EActMotionSourceType::MontageRootMotion;
	Params.BasisMode = BasisMode;
	Params.Duration = Duration;
	Params.Montage = Montage;
	Params.StartTrackPosition = StartTrackPosition;
	Params.EndTrackPosition = EndTrackPosition;
	Params.bApplyRootMotionRotation = bApplyRootMotionRotation;
	Params.bRespectHigherPriorityRotation = bRespectHigherPriorityRotation;
	Params.bIgnoreZAccumulate = bIgnoreZAccumulate;
	Params.Rotation = Rotation;
	Params.Provenance = Provenance;
	Params.SyncId = SyncId;

	USkeletalMeshComponent* BasisMesh = nullptr;
	if ((BasisMode == EActMotionBasisMode::MeshStartFrozen || BasisMode == EActMotionBasisMode::MeshLive) && CharacterOwner)
	{
		BasisMesh = CharacterOwner->GetMesh();
	}

	return ApplyMotionInternal(Params, BasisMesh, ExistingHandle);
}

bool UActCharacterMovementComponent::StopMotion(const int32 Handle)
{
	return StopMotionInternal(Handle);
}

bool UActCharacterMovementComponent::StopMotionBySyncId(const int32 SyncId)
{
	if (SyncId == INDEX_NONE)
	{
		return false;
	}

	if (const int32* Handle = MotionStateMapBySyncId.Find(SyncId))
	{
		return StopMotionInternal(*Handle);
	}

	return false;
}

void UActCharacterMovementComponent::StopAllMotion()
{
	if (MotionStateMap.IsEmpty())
	{
		return;
	}

	bool bNeedsReplicationRefresh = false;
	bool bNeedsPredictionRevision = false;
	for (const TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		bNeedsReplicationRefresh |= IsReplicatedMotionSource(Pair.Value.Params);
		bNeedsPredictionRevision |= ShouldAffectPredictionRevision(Pair.Value.Params);
	}

	MotionStateMap.Empty();
	MotionStateMapBySyncId.Empty();
	ConfirmedOwnerPredictedMotionSyncIds.Reset();
	ActiveRotationHandle = INDEX_NONE;
	LockOnRotationHandle = INDEX_NONE;
	PendingMotionDisplacement = FVector::ZeroVector;
	PendingMotionRotation = FQuat::Identity;
	if (bNeedsPredictionRevision)
	{
		++MotionStateRevision;
	}
	RefreshNetworkCorrectionMode();
	if (bNeedsReplicationRefresh)
	{
		RequestReplicatedMotionRefresh();
	}
}

bool UActCharacterMovementComponent::HasActiveMotion() const
{
	return !MotionStateMap.IsEmpty();
}

bool UActCharacterMovementComponent::HasMotionBySyncId(const int32 SyncId) const
{
	return MotionStateMapBySyncId.Contains(SyncId);
}

void UActCharacterMovementComponent::ExtendExternalLaunchCorrectionGrace(const float GraceDuration)
{
	if (GraceDuration <= UE_SMALL_NUMBER)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		ExternalLaunchCorrectionGraceEndTime = FMath::Max(ExternalLaunchCorrectionGraceEndTime, World->GetTimeSeconds() + GraceDuration);
		RefreshNetworkCorrectionMode();
	}
}

bool UActCharacterMovementComponent::HasActiveReplicatedExternalMotion() const
{
	for (const TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		const FActMotionState& State = Pair.Value;
		if (State.Params.Provenance != EActMotionProvenance::AuthorityExternal &&
			State.Params.Provenance != EActMotionProvenance::ReplicatedExternal)
		{
			continue;
		}

		if (!DoesMotionPassModeFilter(State.Params))
		{
			continue;
		}

		if (State.Params.HasTranslation() || State.Params.HasLaunch() || State.Params.HasRotation())
		{
			return true;
		}
	}

	return false;
}

bool UActCharacterMovementComponent::ShouldSuppressSharedReplication(const FVector& ServerLocation) const
{
	if (!UpdatedComponent || !CharacterOwner || CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		return false;
	}

	if (!HasActiveReplicatedExternalMotion())
	{
		return false;
	}

	const FVector LocalLocation = UpdatedComponent->GetComponentLocation();
	const FVector LocationDelta = ServerLocation - LocalLocation;
	const FVector HorizontalDelta(LocationDelta.X, LocationDelta.Y, 0.0f);

	constexpr float HorizontalTolerance = 70.0f;
	constexpr float VerticalTolerance = 72.0f;
	constexpr float MaxDistanceTolerance = 96.0f;

	return HorizontalDelta.SizeSquared() <= FMath::Square(HorizontalTolerance) &&
		FMath::Abs(LocationDelta.Z) <= VerticalTolerance &&
		LocationDelta.SizeSquared() <= FMath::Square(MaxDistanceTolerance);
}

void UActCharacterMovementComponent::ApplySuppressedSharedReplication(const FVector& ServerLocation, const FVector& ServerVelocity)
{
	if (!UpdatedComponent)
	{
		return;
	}

	Velocity = ServerVelocity;

	const FVector LocalLocation = UpdatedComponent->GetComponentLocation();
	const FVector LocationDelta = ServerLocation - LocalLocation;
	if (LocationDelta.IsNearlyZero(1.0f))
	{
		return;
	}

	// Keep simulated proxies close to the server-authored trajectory without reintroducing hard snaps.
	FVector SoftCorrection = LocationDelta;
	SoftCorrection.X *= 0.18f;
	SoftCorrection.Y *= 0.18f;
	SoftCorrection.Z *= 0.35f;

	if (SoftCorrection.IsNearlyZero(0.1f))
	{
		return;
	}

	FHitResult Hit;
	bApplyingMotion = true;
	SafeMoveUpdatedComponent(SoftCorrection, UpdatedComponent->GetComponentRotation(), true, Hit);
	bApplyingMotion = false;
	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(SoftCorrection, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}
}

void UActCharacterMovementComponent::SyncReplicatedMotions(const TArray<FActReplicatedMotion>& ReplicatedMotions)
{
	if (CharacterOwner && CharacterOwner->HasAuthority())
	{
		return;
	}

	TSet<int32> IncomingSyncIds;
	const bool bLocallyControlled = CharacterOwner && CharacterOwner->IsLocallyControlled();

	for (const FActReplicatedMotion& Entry : ReplicatedMotions)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		IncomingSyncIds.Add(Entry.Params.SyncId);

		if (bLocallyControlled && Entry.Params.Provenance == EActMotionProvenance::OwnerPredicted)
		{
			if (const int32* ExistingHandle = MotionStateMapBySyncId.Find(Entry.Params.SyncId))
			{
				if (const FActMotionState* ExistingState = MotionStateMap.Find(*ExistingHandle))
				{
					if (ExistingState->Params.Provenance == EActMotionProvenance::OwnerPredicted)
					{
						ConfirmedOwnerPredictedMotionSyncIds.Add(Entry.Params.SyncId);
						continue;
					}
				}
			}
		}

		FActMotionParams Params = Entry.Params;
		Params.Provenance = EActMotionProvenance::ReplicatedExternal;

		USkeletalMeshComponent* Mesh = nullptr;
		if ((Params.BasisMode == EActMotionBasisMode::MeshStartFrozen || Params.BasisMode == EActMotionBasisMode::MeshLive) && CharacterOwner)
		{
			Mesh = CharacterOwner->GetMesh();
		}

		const float InitialElapsedTime = FMath::Max(GetReplicatedServerWorldTimeSeconds() - Entry.ServerStartTimeSeconds, 0.0f);
		const int32 ExistingHandle = MotionStateMapBySyncId.FindRef(Params.SyncId);
		float PreviousElapsedTime = 0.0f;
		if (const FActMotionState* ExistingState = MotionStateMap.Find(ExistingHandle))
		{
			PreviousElapsedTime = ExistingState->ElapsedTime;
		}

		const int32 Handle = ApplyMotionInternal(Params, Mesh, ExistingHandle, InitialElapsedTime);
		if (FActMotionState* State = MotionStateMap.Find(Handle))
		{
			State->ServerStartTimeSeconds = Entry.ServerStartTimeSeconds;
			State->StartLocation = Entry.StartLocation;
			State->bHasStartLocation = true;
			State->FrozenBasisRotation = Entry.FrozenBasisRotation;
			State->bHasFrozenBasisRotation = Params.BasisMode == EActMotionBasisMode::ActorStartFrozen || Params.BasisMode == EActMotionBasisMode::MeshStartFrozen;
			State->FrozenFacingDirection = Entry.FrozenFacingDirection.GetSafeNormal();
			State->bHasFrozenFacingDirection = Params.Rotation.bFreezeDirectionAtStart && !State->FrozenFacingDirection.IsNearlyZero();
			ApplyMotionLaunch(*State, InitialElapsedTime);
			ApplyReplicatedMotionCatchUp(*State, PreviousElapsedTime, InitialElapsedTime);
		}
	}

	TArray<int32> HandlesToRemove;
	HandlesToRemove.Reserve(MotionStateMap.Num());
	for (const TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		const FActMotionState& State = Pair.Value;
		if (State.Params.SyncId == INDEX_NONE)
		{
			continue;
		}

		if (State.Params.Provenance == EActMotionProvenance::ReplicatedExternal && !IncomingSyncIds.Contains(State.Params.SyncId))
		{
			HandlesToRemove.Add(Pair.Key);
			continue;
		}

		if (bLocallyControlled &&
			State.Params.Provenance == EActMotionProvenance::OwnerPredicted &&
			ConfirmedOwnerPredictedMotionSyncIds.Contains(State.Params.SyncId) &&
			!IncomingSyncIds.Contains(State.Params.SyncId))
		{
			HandlesToRemove.Add(Pair.Key);
		}
	}

	if (!HandlesToRemove.IsEmpty())
	{
		for (const int32 Handle : HandlesToRemove)
		{
			if (const FActMotionState* State = MotionStateMap.Find(Handle); State && State->Params.SyncId != INDEX_NONE)
			{
				ConfirmedOwnerPredictedMotionSyncIds.Remove(State->Params.SyncId);
			}
		}
		RemoveMotionEntries(HandlesToRemove);
	}

	RefreshActiveRotationTask();
}

void UActCharacterMovementComponent::CapturePredictedMotionSnapshots(TArray<FActPredictedMotionSnapshot, TInlineAllocator<4>>& OutSnapshots) const
{
	OutSnapshots.Reset();
	OutSnapshots.Reserve(MotionStateMap.Num());

	for (const TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		const FActMotionState& State = Pair.Value;
		if (State.Params.Provenance != EActMotionProvenance::OwnerPredicted)
		{
			continue;
		}

		FActPredictedMotionSnapshot& Snapshot = OutSnapshots.AddDefaulted_GetRef();
		Snapshot.LocalHandle = Pair.Key;
		Snapshot.SyncId = State.Params.SyncId;
		Snapshot.Params = State.Params;
		Snapshot.ElapsedTime = State.ElapsedTime;
		Snapshot.RotationElapsedTime = State.RotationElapsedTime;
		Snapshot.FrozenBasisYawDegrees = State.bHasFrozenBasisRotation ? State.FrozenBasisRotation.Yaw : 0.0f;
		Snapshot.FrozenFacingDirection = State.bHasFrozenFacingDirection ? State.FrozenFacingDirection : FVector::ZeroVector;
		Snapshot.bRotationCompleted = State.bRotationCompleted;
		Snapshot.bRootMotionRotationSuppressed = State.bRootMotionRotationSuppressed;
	}
}

void UActCharacterMovementComponent::RestorePredictedMotionSnapshots(const TArray<FActPredictedMotionSnapshot, TInlineAllocator<4>>& Snapshots, const bool bNetworkSynchronizedOnly)
{
	auto AreMotionParamsEquivalent = [](const FActMotionParams& A, const FActMotionParams& B)
	{
		return A.SourceType == B.SourceType &&
			A.BasisMode == B.BasisMode &&
			A.Velocity.Equals(B.Velocity) &&
			A.LaunchVelocity.Equals(B.LaunchVelocity) &&
			A.bOverrideLaunchXY == B.bOverrideLaunchXY &&
			A.bOverrideLaunchZ == B.bOverrideLaunchZ &&
			FMath::IsNearlyEqual(A.Duration, B.Duration) &&
			A.ModeFilter == B.ModeFilter &&
			A.CurveType == B.CurveType &&
			A.Curve == B.Curve &&
			A.Montage == B.Montage &&
			FMath::IsNearlyEqual(A.StartTrackPosition, B.StartTrackPosition) &&
			FMath::IsNearlyEqual(A.EndTrackPosition, B.EndTrackPosition) &&
			A.bApplyRootMotionRotation == B.bApplyRootMotionRotation &&
			A.bRespectHigherPriorityRotation == B.bRespectHigherPriorityRotation &&
			A.bIgnoreZAccumulate == B.bIgnoreZAccumulate &&
			A.Rotation.SourceType == B.Rotation.SourceType &&
			A.Rotation.TargetActor == B.Rotation.TargetActor &&
			A.Rotation.ActorMode == B.Rotation.ActorMode &&
			A.Rotation.Direction.Equals(B.Rotation.Direction) &&
			FMath::IsNearlyEqual(A.Rotation.AcceptableYawError, B.Rotation.AcceptableYawError) &&
			A.Rotation.bClearOnReached == B.Rotation.bClearOnReached &&
			A.Rotation.bFreezeDirectionAtStart == B.Rotation.bFreezeDirectionAtStart &&
			A.Rotation.bIsAdditive == B.Rotation.bIsAdditive &&
			A.Rotation.Priority == B.Rotation.Priority &&
			A.Provenance == B.Provenance &&
			A.SyncId == B.SyncId;
	};

	TArray<int32, TInlineAllocator<4>> IncomingSyncIds;
	TArray<int32, TInlineAllocator<4>> IncomingLocalHandles;
	IncomingSyncIds.Reserve(Snapshots.Num());
	IncomingLocalHandles.Reserve(Snapshots.Num());

	for (const FActPredictedMotionSnapshot& Snapshot : Snapshots)
	{
		if (Snapshot.SyncId != INDEX_NONE)
		{
			IncomingSyncIds.AddUnique(Snapshot.SyncId);
		}
		if (!bNetworkSynchronizedOnly && Snapshot.LocalHandle != INDEX_NONE)
		{
			IncomingLocalHandles.AddUnique(Snapshot.LocalHandle);
		}
	}

	TArray<int32> HandlesToRemove;
	HandlesToRemove.Reserve(MotionStateMap.Num());
	for (const TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		const FActMotionState& State = Pair.Value;
		if (State.Params.Provenance != EActMotionProvenance::OwnerPredicted)
		{
			continue;
		}

		const bool bTrackBySyncId = State.Params.SyncId != INDEX_NONE;
		const bool bTrackByHandle = !bTrackBySyncId && !bNetworkSynchronizedOnly;
		if ((bTrackBySyncId && !IncomingSyncIds.Contains(State.Params.SyncId)) ||
			(bTrackByHandle && !IncomingLocalHandles.Contains(Pair.Key)))
		{
			HandlesToRemove.Add(Pair.Key);
		}
	}

	RemoveMotionEntries(HandlesToRemove);

	bool bChanged = false;
	for (const FActPredictedMotionSnapshot& Snapshot : Snapshots)
	{
		int32 ExistingHandle = INDEX_NONE;
		if (Snapshot.SyncId != INDEX_NONE)
		{
			if (const int32* ExistingSyncHandle = MotionStateMapBySyncId.Find(Snapshot.SyncId))
			{
				ExistingHandle = *ExistingSyncHandle;
			}
		}
		else if (!bNetworkSynchronizedOnly)
		{
			ExistingHandle = Snapshot.LocalHandle;
		}

		FActMotionState* ExistingState = ExistingHandle != INDEX_NONE ? MotionStateMap.Find(ExistingHandle) : nullptr;
		if (!ExistingState)
		{
			if (bNetworkSynchronizedOnly)
			{
				continue;
			}

			USkeletalMeshComponent* Mesh = nullptr;
			if ((Snapshot.Params.BasisMode == EActMotionBasisMode::MeshStartFrozen || Snapshot.Params.BasisMode == EActMotionBasisMode::MeshLive) && CharacterOwner)
			{
				Mesh = CharacterOwner->GetMesh();
			}

			ExistingHandle = ApplyMotionInternal(Snapshot.Params, Mesh, Snapshot.LocalHandle, Snapshot.ElapsedTime);
			ExistingState = MotionStateMap.Find(ExistingHandle);
		}
		else if (!bNetworkSynchronizedOnly && !AreMotionParamsEquivalent(ExistingState->Params, Snapshot.Params))
		{
			USkeletalMeshComponent* Mesh = nullptr;
			if ((Snapshot.Params.BasisMode == EActMotionBasisMode::MeshStartFrozen || Snapshot.Params.BasisMode == EActMotionBasisMode::MeshLive) && CharacterOwner)
			{
				Mesh = CharacterOwner->GetMesh();
			}

			ExistingHandle = ApplyMotionInternal(Snapshot.Params, Mesh, ExistingHandle, Snapshot.ElapsedTime);
			ExistingState = MotionStateMap.Find(ExistingHandle);
		}

		if (ExistingState)
		{
			ExistingState->ElapsedTime = Snapshot.ElapsedTime;
			ExistingState->RotationElapsedTime = Snapshot.RotationElapsedTime;
			ExistingState->bRotationCompleted = Snapshot.bRotationCompleted;
			ExistingState->bRootMotionRotationSuppressed = Snapshot.bRootMotionRotationSuppressed;
			if (ExistingState->Params.BasisMode == EActMotionBasisMode::ActorStartFrozen || ExistingState->Params.BasisMode == EActMotionBasisMode::MeshStartFrozen)
			{
				ExistingState->FrozenBasisRotation = FRotator(0.0f, Snapshot.FrozenBasisYawDegrees, 0.0f);
				ExistingState->bHasFrozenBasisRotation = true;
			}
			ExistingState->FrozenFacingDirection = Snapshot.FrozenFacingDirection.GetSafeNormal();
			ExistingState->bHasFrozenFacingDirection = !ExistingState->FrozenFacingDirection.IsNearlyZero();
			bChanged = true;
		}
	}

	if (bChanged)
	{
		++MotionStateRevision;
		RefreshActiveRotationTask();
	}
}

void UActCharacterMovementComponent::BuildReplicatedMotionEntries(TArray<FActReplicatedMotion>& OutEntries) const
{
	OutEntries.Reset();
	OutEntries.Reserve(MotionStateMap.Num());

	for (const TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		const FActMotionState& State = Pair.Value;
		if (!IsReplicatedMotionSource(State.Params))
		{
			continue;
		}

		FActReplicatedMotion& Entry = OutEntries.AddDefaulted_GetRef();
		Entry.Params = State.Params;
		Entry.ServerStartTimeSeconds = State.ServerStartTimeSeconds;
		Entry.StartLocation = State.bHasStartLocation ? State.StartLocation : FVector::ZeroVector;
		Entry.FrozenBasisRotation = State.FrozenBasisRotation;
		Entry.FrozenFacingDirection = State.bHasFrozenFacingDirection ? State.FrozenFacingDirection : FVector::ZeroVector;
	}

	OutEntries.Sort([](const FActReplicatedMotion& A, const FActReplicatedMotion& B)
	{
		return A.Params.SyncId < B.Params.SyncId;
	});
}

FNetworkPredictionData_Client* UActCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);

	if (!ClientPredictionData)
	{
		UActCharacterMovementComponent* MutableThis = const_cast<UActCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FActNetworkPredictionData_Client_Character(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = NetworkMaxSmoothUpdateDistance;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = NetworkNoSmoothUpdateDistance;
	}

	return ClientPredictionData;
}

void UActCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	if (const FActCharacterNetworkMoveData* ActMoveData = static_cast<const FActCharacterNetworkMoveData*>(GetCurrentNetworkMoveData()))
	{
		RestorePredictedMotionSnapshots(ActMoveData->MotionSnapshots, true);
	}

	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

void UActCharacterMovementComponent::SimulateMovement(float DeltaTime)
{
	if (bHasReplicatedAcceleration)
	{
		const FVector OriginalAcceleration = Acceleration;
		Super::SimulateMovement(DeltaTime);
		Acceleration = OriginalAcceleration;
	}
	else
	{
		Super::SimulateMovement(DeltaTime);
	}
}

bool UActCharacterMovementComponent::CanAttemptJump() const
{
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling());
}

void UActCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	UCharacterMovementComponent::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	const bool bOldHasAcceleration = bHasAcceleration;
	const bool bOldIsOnGround = bIsOnGround;
	bHasAcceleration = GetHasAcceleration();
	bIsOnGround = IsMovingOnGround();

	if (bHasAcceleration != bOldHasAcceleration)
	{
		OnAccelerationStateChanged.Broadcast(bOldHasAcceleration, bHasAcceleration);
	}

	if (bIsOnGround != bOldIsOnGround)
	{
		OnGroundStateChanged.Broadcast(bOldIsOnGround, bIsOnGround);
	}

	UpdateLockOnRotationTask();
	RefreshActiveRotationTask();
	ApplyPendingMotion(DeltaSeconds);

	if (ExternalLaunchCorrectionGraceEndTime > 0.0f)
	{
		RefreshNetworkCorrectionMode();
	}
}

void UActCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	UpdateLockOnRotationTask();
	RefreshActiveRotationTask();

	if (HasBlockingMotionRotation())
	{
		return;
	}

	Super::PhysicsRotation(DeltaTime);
}

void UActCharacterMovementComponent::UpdateLockOnRotationTask()
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		if (LockOnRotationHandle != INDEX_NONE)
		{
			StopMotionInternal(LockOnRotationHandle);
		}
		return;
	}

	UActBattleComponent* BattleComponent = CharacterOwner->FindComponentByClass<UActBattleComponent>();
	const bool bLockOnActive = BattleComponent && BattleComponent->IsLockOnActive();
	const bool bHasMovementAcceleration = GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER;
	AActor* LockOnTarget = BattleComponent ? BattleComponent->GetLockOnTarget() : nullptr;

	if (!bLockOnActive || !bHasMovementAcceleration || !IsValid(LockOnTarget))
	{
		if (LockOnRotationHandle != INDEX_NONE)
		{
			StopMotionInternal(LockOnRotationHandle);
		}
		return;
	}

	FActMotionRotationParams RotationParams;
	RotationParams.SourceType = EActMotionRotationSourceType::Actor;
	RotationParams.TargetActor = LockOnTarget;
	RotationParams.ActorMode = EActMotionRotationActorMode::FaceTarget;
	RotationParams.AcceptableYawError = 1.0f;
	RotationParams.bClearOnReached = false;
	RotationParams.bFreezeDirectionAtStart = false;
	RotationParams.bIsAdditive = false;
	RotationParams.Priority = EActMotionRotationPriority::LockOn;
	const float RotationDuration = LockOnRotationTaskDuration;

	bool bNeedsUpdate = true;
	if (const FActMotionState* ExistingState = MotionStateMap.Find(LockOnRotationHandle))
	{
		const FActMotionRotationParams& ExistingRotation = ExistingState->Params.Rotation;
		bNeedsUpdate =
			ExistingState->Params.Provenance != EActMotionProvenance::LocalRuntime ||
			ExistingRotation.SourceType != RotationParams.SourceType ||
			ExistingRotation.TargetActor.Get() != RotationParams.TargetActor.Get() ||
			ExistingRotation.ActorMode != RotationParams.ActorMode ||
			!FMath::IsNearlyEqual(ExistingState->Params.Duration, RotationDuration) ||
			ExistingRotation.Priority != RotationParams.Priority;
	}

	if (!bNeedsUpdate)
	{
		return;
	}

	FActMotionParams Params;
	Params.Duration = RotationDuration;
	Params.ModeFilter = EActMotionModeFilter::Any;
	Params.Rotation = RotationParams;
	Params.Provenance = EActMotionProvenance::LocalRuntime;
	LockOnRotationHandle = ApplyMotionInternal(Params, nullptr, LockOnRotationHandle, 0.0f);
}

bool UActCharacterMovementComponent::CanStateDriveRotationTask(const FActMotionState& State) const
{
	return DoesMotionPassModeFilter(State.Params) &&
		!State.bRotationCompleted &&
		State.Params.Rotation.IsValidRequest();
}

void UActCharacterMovementComponent::RefreshActiveRotationTask()
{
	int32 BestHandle = INDEX_NONE;
	EActMotionRotationPriority BestPriority = EActMotionRotationPriority::Locomotion;

	for (TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		FActMotionState& State = Pair.Value;
		if (!CanStateDriveRotationTask(State))
		{
			State.bRotationPaused = false;
			continue;
		}

		const EActMotionRotationPriority Priority = State.Params.Rotation.Priority;
		if (BestHandle == INDEX_NONE ||
			static_cast<uint8>(Priority) > static_cast<uint8>(BestPriority) ||
			(static_cast<uint8>(Priority) == static_cast<uint8>(BestPriority) && Pair.Key > BestHandle))
		{
			BestHandle = Pair.Key;
			BestPriority = Priority;
		}
	}

	ActiveRotationHandle = BestHandle;
	for (TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		FActMotionState& State = Pair.Value;
		State.bRotationPaused = CanStateDriveRotationTask(State) && Pair.Key != ActiveRotationHandle;
	}
}

bool UActCharacterMovementComponent::TickActiveRotationTask(const float DeltaSeconds, FRotator& InOutFinalRotation)
{
	if (DeltaSeconds <= UE_SMALL_NUMBER || ActiveRotationHandle == INDEX_NONE)
	{
		return false;
	}

	FActMotionState* ActiveState = MotionStateMap.Find(ActiveRotationHandle);
	if (!ActiveState || !CanStateDriveRotationTask(*ActiveState) || ActiveState->bRotationPaused)
	{
		ActiveRotationHandle = INDEX_NONE;
		return false;
	}

	const FVector FacingDirection = ResolveMotionFacingDirection(*ActiveState);
	if (FacingDirection.IsNearlyZero())
	{
		ActiveState->bRotationCompleted = true;
		if (!ActiveState->Params.HasTranslation())
		{
			TArray<int32> HandlesToRemove;
			HandlesToRemove.Add(ActiveRotationHandle);
			RemoveMotionEntries(HandlesToRemove);
		}
		else if (ShouldAffectPredictionRevision(ActiveState->Params))
		{
			++MotionStateRevision;
		}
		RefreshActiveRotationTask();
		return false;
	}

	FRotator DesiredRotation = FacingDirection.Rotation();
	DesiredRotation.Pitch = 0.0f;
	DesiredRotation.Roll = 0.0f;

	const FQuat PreviousRotationQuat = InOutFinalRotation.Quaternion();
	const float RotationDuration = ActiveState->Params.Duration > UE_SMALL_NUMBER ? ActiveState->Params.Duration : 0.0f;
	if (RotationDuration <= UE_SMALL_NUMBER)
	{
		InOutFinalRotation = DesiredRotation;
	}
	else
	{
		const float RemainingRotationTime = FMath::Max(RotationDuration - ActiveState->RotationElapsedTime, DeltaSeconds);
		const float StepAlpha = FMath::Clamp(DeltaSeconds / RemainingRotationTime, 0.0f, 1.0f);
		InOutFinalRotation = FQuat::Slerp(PreviousRotationQuat, DesiredRotation.Quaternion(), StepAlpha).Rotator();
	}

	if (ActiveState->Params.Rotation.bIsAdditive)
	{
		const FQuat RotationDelta = InOutFinalRotation.Quaternion() * PreviousRotationQuat.Inverse();
		ApplyAdditiveRotationToActiveMotionBases(ActiveRotationHandle, RotationDelta);
	}
	else
	{
		ApplyAbsoluteRotationToActiveMotionBases(ActiveRotationHandle, InOutFinalRotation);
	}

	ActiveState->RotationElapsedTime += DeltaSeconds;

	bool bRotationFinished = false;
	if (ActiveState->Params.Duration > 0.0f &&
		ActiveState->RotationElapsedTime >= ActiveState->Params.Duration - UE_SMALL_NUMBER)
	{
		bRotationFinished = true;
	}

	if (bRotationFinished)
	{
		ActiveState->bRotationCompleted = true;
		if (!ActiveState->Params.HasTranslation())
		{
			TArray<int32> HandlesToRemove;
			HandlesToRemove.Add(ActiveRotationHandle);
			RemoveMotionEntries(HandlesToRemove);
		}
		else if (ShouldAffectPredictionRevision(ActiveState->Params))
		{
			++MotionStateRevision;
		}

		RefreshActiveRotationTask();
	}

	return true;
}

int32 UActCharacterMovementComponent::ApplyMotionInternal(FActMotionParams Params, USkeletalMeshComponent* Mesh, const int32 ExistingHandle, const float InitialElapsedTime)
{
	if (Params.Duration == 0.0f)
	{
		return INDEX_NONE;
	}

	if (Params.SyncId == INDEX_NONE &&
		Params.Provenance != EActMotionProvenance::ReplicatedExternal &&
		Params.Provenance != EActMotionProvenance::LocalRuntime)
	{
		if ((CharacterOwner && CharacterOwner->HasAuthority()) || Params.Provenance == EActMotionProvenance::OwnerPredicted)
		{
			Params.SyncId = GenerateMotionSyncId();
		}
	}

	const int32 Handle = AcquireMotionHandle(ExistingHandle, Params.SyncId);

	FActMotionState& State = MotionStateMap.FindOrAdd(Handle);
	RemoveMotionMappings(Handle, &State);
	State.Handle = Handle;
	State.Params = Params;
	State.BasisMesh = Mesh;
	State.ElapsedTime = Params.Duration > 0.0f
		? FMath::Clamp(InitialElapsedTime, 0.0f, Params.Duration)
		: FMath::Max(InitialElapsedTime, 0.0f);
	State.RotationElapsedTime = Params.Duration > 0.0f
		? FMath::Clamp(InitialElapsedTime, 0.0f, Params.Duration)
		: FMath::Max(InitialElapsedTime, 0.0f);
	State.ServerStartTimeSeconds = GetReplicatedServerWorldTimeSeconds() - State.ElapsedTime;
	State.bRotationCompleted = false;
	State.bRotationPaused = false;
	State.bRootMotionRotationSuppressed = false;
	State.bHasFrozenBasisRotation = false;
	State.bHasFrozenFacingDirection = false;
	State.FrozenBasisRotation = FRotator::ZeroRotator;
	State.FrozenFacingDirection = FVector::ZeroVector;
	State.bLaunchApplied = false;
	State.StartLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	State.bHasStartLocation = UpdatedComponent != nullptr;

	if (Params.Provenance != EActMotionProvenance::ReplicatedExternal)
	{
		CaptureRuntimeBasis(State);
		ApplyMotionLaunch(State, State.ElapsedTime);
	}

	if (Params.SyncId != INDEX_NONE)
	{
		MotionStateMapBySyncId.FindOrAdd(Params.SyncId) = Handle;
	}

	if (ShouldAffectPredictionRevision(Params))
	{
		++MotionStateRevision;
	}
	RefreshNetworkCorrectionMode();
	RefreshActiveRotationTask();
	if (IsReplicatedMotionSource(Params))
	{
		RequestReplicatedMotionRefresh();
	}
	return Handle;
}

bool UActCharacterMovementComponent::StopMotionInternal(const int32 Handle)
{
	if (Handle == INDEX_NONE)
	{
		return false;
	}

	const FActMotionState* RemovedState = MotionStateMap.Find(Handle);
	if (!RemovedState)
	{
		return false;
	}

	const FActMotionParams RemovedParams = RemovedState->Params;

	if (RemovedParams.SyncId != INDEX_NONE)
	{
		ConfirmedOwnerPredictedMotionSyncIds.Remove(RemovedParams.SyncId);
	}

	RemoveMotionMappings(Handle, RemovedState);
	const bool bRemoved = MotionStateMap.Remove(Handle) > 0;
	if (!bRemoved)
	{
		return false;
	}

	if (Handle == ActiveRotationHandle)
	{
		ActiveRotationHandle = INDEX_NONE;
	}
	if (Handle == LockOnRotationHandle)
	{
		LockOnRotationHandle = INDEX_NONE;
	}

	if (ShouldAffectPredictionRevision(RemovedParams))
	{
		++MotionStateRevision;
	}
	RefreshNetworkCorrectionMode();
	RefreshActiveRotationTask();
	if (IsReplicatedMotionSource(RemovedParams))
	{
		RequestReplicatedMotionRefresh();
	}
	return true;
}

FVector UActCharacterMovementComponent::ConsumeMotionDisplacement(float DeltaSeconds, FActResolvedMotionRotation& OutRotation)
{
	FVector TotalDisplacement = FVector::ZeroVector;
	TArray<int32> HandlesToRemove;
	HandlesToRemove.Reserve(MotionStateMap.Num());
	const bool bHasActiveExplicitRotation = ActiveRotationHandle != INDEX_NONE;

	for (TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		const int32 Handle = Pair.Key;
		FActMotionState& State = Pair.Value;

		if (!DoesMotionPassModeFilter(State.Params))
		{
			HandlesToRemove.Add(Handle);
			continue;
		}

		const bool bInfiniteDuration = State.Params.Duration < 0.0f;
		const float PreviousElapsedTime = State.ElapsedTime;
		const float RemainingDuration = bInfiniteDuration ? DeltaSeconds : FMath::Max(State.Params.Duration - PreviousElapsedTime, 0.0f);
		const bool bTranslationExpired = !bInfiniteDuration && RemainingDuration <= UE_SMALL_NUMBER;
		if (bTranslationExpired)
		{
			if (!State.Params.HasRotation() || State.bRotationCompleted)
			{
				HandlesToRemove.Add(Handle);
				continue;
			}
		}

		const float EffectiveDeltaTime = bTranslationExpired ? DeltaSeconds : (bInfiniteDuration ? DeltaSeconds : FMath::Min(DeltaSeconds, RemainingDuration));
		const float NewElapsedTime = PreviousElapsedTime + EffectiveDeltaTime;

		FQuat RootMotionRotationDelta = FQuat::Identity;
		const FVector MotionDisplacement = bTranslationExpired
			? FVector::ZeroVector
			: ResolveMotionDisplacement(State, PreviousElapsedTime, NewElapsedTime, EffectiveDeltaTime, RootMotionRotationDelta);
		if (MotionDisplacement.ContainsNaN())
		{
			UE_LOG(LogAct, Warning, TEXT("Removing invalid Motion. Owner=%s Handle=%d SyncId=%d"), *GetNameSafe(CharacterOwner), Handle, State.Params.SyncId);
			HandlesToRemove.Add(Handle);
			continue;
		}

		TotalDisplacement += MotionDisplacement;
		if (!RootMotionRotationDelta.Equals(FQuat::Identity))
		{
			const bool bShouldSuppressRootMotionRotation =
				State.Params.SourceType == EActMotionSourceType::MontageRootMotion &&
				State.Params.bApplyRootMotionRotation &&
				State.Params.bRespectHigherPriorityRotation &&
				bHasActiveExplicitRotation;
			State.bRootMotionRotationSuppressed = bShouldSuppressRootMotionRotation;

			if (!State.bRootMotionRotationSuppressed)
			{
				OutRotation.bHasRootMotionRotation = true;
				OutRotation.RootMotionDelta = RootMotionRotationDelta * OutRotation.RootMotionDelta;
			}
		}

		State.ElapsedTime = NewElapsedTime;
		if (!bInfiniteDuration && State.ElapsedTime >= State.Params.Duration - UE_SMALL_NUMBER)
		{
			if (!State.Params.HasRotation() || State.bRotationCompleted)
			{
				HandlesToRemove.Add(Handle);
			}
		}
	}

	RemoveMotionEntries(HandlesToRemove);
	return TotalDisplacement;
}

float UActCharacterMovementComponent::EvaluateMotionScale(const FActMotionParams& Params, const float ElapsedTime, const float Duration) const
{
	if (Duration <= UE_SMALL_NUMBER)
	{
		return 1.0f;
	}

	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	switch (Params.CurveType)
	{
	case EActMotionCurveType::Constant:
		return 1.0f;
	case EActMotionCurveType::EaseIn:
		return Alpha * Alpha;
	case EActMotionCurveType::EaseOut:
		return 1.0f - FMath::Square(1.0f - Alpha);
	case EActMotionCurveType::EaseInOut:
		return FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	case EActMotionCurveType::CustomCurve:
		return Params.Curve ? Params.Curve->GetFloatValue(Alpha) : 1.0f;
	default:
		return 1.0f;
	}
}

bool UActCharacterMovementComponent::DoesMotionPassModeFilter(const FActMotionParams& Params) const
{
	switch (Params.ModeFilter)
	{
	case EActMotionModeFilter::Any:
		return true;
	case EActMotionModeFilter::GroundedOnly:
		return IsMovingOnGround();
	case EActMotionModeFilter::AirOnly:
		return !IsMovingOnGround();
	case EActMotionModeFilter::FallingOnly:
		return MovementMode == MOVE_Falling;
	case EActMotionModeFilter::WalkingOnly:
		return MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking;
	default:
		return true;
	}
}

void UActCharacterMovementComponent::ApplyMotionLaunch(FActMotionState& State, const float ElapsedTime)
{
	if (State.bLaunchApplied || !State.Params.HasLaunch())
	{
		return;
	}

	const FVector WorldLaunchVelocity = ResolveMotionLaunchVelocity(State, ElapsedTime);
	if (WorldLaunchVelocity.IsNearlyZero())
	{
		State.bLaunchApplied = true;
		return;
	}

	SetMovementMode(MOVE_Falling);

	if (State.Params.bOverrideLaunchXY)
	{
		Velocity.X = WorldLaunchVelocity.X;
		Velocity.Y = WorldLaunchVelocity.Y;
	}
	else
	{
		Velocity.X += WorldLaunchVelocity.X;
		Velocity.Y += WorldLaunchVelocity.Y;
	}

	if (State.Params.bOverrideLaunchZ)
	{
		Velocity.Z = WorldLaunchVelocity.Z;
	}
	else
	{
		Velocity.Z += WorldLaunchVelocity.Z;
	}

	bForceNextFloorCheck = true;
	State.bLaunchApplied = true;

	if (WorldLaunchVelocity.Z > UE_SMALL_NUMBER)
	{
		const float GravityMagnitude = FMath::Abs(GetGravityZ());
		const float TimeToApex = GravityMagnitude > UE_SMALL_NUMBER ? WorldLaunchVelocity.Z / GravityMagnitude : 0.0f;
		const float GraceDuration = TimeToApex > UE_SMALL_NUMBER ? (TimeToApex * 2.0f) + 0.12f : 0.18f;
		ExtendExternalLaunchCorrectionGrace(GraceDuration);
	}
}

FVector UActCharacterMovementComponent::ResolveMotionLaunchVelocity(const FActMotionState& State, const float ElapsedTime) const
{
	if (!State.Params.HasLaunch())
	{
		return FVector::ZeroVector;
	}

	FVector WorldLaunchVelocity = ResolveMotionBasisTransform(State).TransformVectorNoScale(State.Params.LaunchVelocity);
	if (State.Params.bOverrideLaunchZ && ElapsedTime > UE_SMALL_NUMBER)
	{
		WorldLaunchVelocity.Z += GetGravityZ() * ElapsedTime;
	}

	return WorldLaunchVelocity;
}

FVector UActCharacterMovementComponent::ResolveMotionLaunchDisplacement(const FActMotionState& State, const float ElapsedTime) const
{
	if (!State.Params.HasLaunch() || ElapsedTime <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const FVector InitialWorldLaunchVelocity = ResolveMotionBasisTransform(State).TransformVectorNoScale(State.Params.LaunchVelocity);
	FVector Displacement = FVector::ZeroVector;

	if (State.Params.bOverrideLaunchXY)
	{
		Displacement.X = InitialWorldLaunchVelocity.X * ElapsedTime;
		Displacement.Y = InitialWorldLaunchVelocity.Y * ElapsedTime;
	}

	if (State.Params.bOverrideLaunchZ)
	{
		Displacement.Z = (InitialWorldLaunchVelocity.Z * ElapsedTime) + (0.5f * GetGravityZ() * FMath::Square(ElapsedTime));
	}
	else
	{
		Displacement.Z = InitialWorldLaunchVelocity.Z * ElapsedTime;
	}

	return Displacement;
}

FVector UActCharacterMovementComponent::ResolveMotionDisplacement(const FActMotionState& State, const float PreviousElapsedTime, const float NewElapsedTime, const float DeltaSeconds, FQuat& OutRootMotionRotationDelta) const
{
	OutRootMotionRotationDelta = FQuat::Identity;

	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	if (State.Params.SourceType == EActMotionSourceType::MontageRootMotion)
	{
		if (!State.Params.Montage || State.Params.Duration <= UE_SMALL_NUMBER)
		{
			return FVector::ZeroVector;
		}

		const float PreviousAlpha = FMath::Clamp(PreviousElapsedTime / State.Params.Duration, 0.0f, 1.0f);
		const float CurrentAlpha = FMath::Clamp(NewElapsedTime / State.Params.Duration, 0.0f, 1.0f);
		const float StartTrackPosition = FMath::Lerp(State.Params.StartTrackPosition, State.Params.EndTrackPosition, PreviousAlpha);
		const float EndTrackPosition = FMath::Lerp(State.Params.StartTrackPosition, State.Params.EndTrackPosition, CurrentAlpha);
		FTransform DeltaTransform = State.Params.Montage->ExtractRootMotionFromTrackRange(StartTrackPosition, EndTrackPosition, FAnimExtractContext());

		if (State.Params.bIgnoreZAccumulate)
		{
			FVector Translation = DeltaTransform.GetTranslation();
			Translation.Z = 0.0f;
			DeltaTransform.SetTranslation(Translation);
		}

		if (State.Params.bApplyRootMotionRotation)
		{
			OutRootMotionRotationDelta = DeltaTransform.GetRotation();
		}

		const FTransform Basis = ResolveMotionBasisTransform(State);
		return Basis.TransformVectorNoScale(DeltaTransform.GetTranslation());
	}

	const FTransform Basis = ResolveMotionBasisTransform(State);
	const float Scale = State.Params.Duration > 0.0f ? EvaluateMotionScale(State.Params, NewElapsedTime, State.Params.Duration) : 1.0f;
	return Basis.TransformVectorNoScale(State.Params.Velocity * Scale) * DeltaSeconds;
}

FTransform UActCharacterMovementComponent::ResolveMotionBasisTransform(const FActMotionState& State) const
{
	switch (State.Params.BasisMode)
	{
	case EActMotionBasisMode::ActorStartFrozen:
	case EActMotionBasisMode::MeshStartFrozen:
		return State.bHasFrozenBasisRotation ? FTransform(State.FrozenBasisRotation) : FTransform::Identity;
	case EActMotionBasisMode::ActorLive:
		return UpdatedComponent ? UpdatedComponent->GetComponentTransform() : FTransform::Identity;
	case EActMotionBasisMode::MeshLive:
		if (USkeletalMeshComponent* Mesh = State.BasisMesh.Get(); !Mesh && CharacterOwner)
		{
			Mesh = CharacterOwner->GetMesh();
			if (Mesh)
			{
				return Mesh->GetComponentTransform();
			}
		}
		else if (Mesh)
		{
			return Mesh->GetComponentTransform();
		}
		return UpdatedComponent ? UpdatedComponent->GetComponentTransform() : FTransform::Identity;
	case EActMotionBasisMode::World:
	default:
		return FTransform::Identity;
	}
}

FVector UActCharacterMovementComponent::ResolveMotionFacingDirection(const FActMotionState& State) const
{
	if (State.bRotationCompleted || !State.Params.Rotation.IsValidRequest())
	{
		return FVector::ZeroVector;
	}

	if (State.bHasFrozenFacingDirection)
	{
		return State.FrozenFacingDirection;
	}

	FVector DesiredDirection = FVector::ZeroVector;
	switch (State.Params.Rotation.SourceType)
	{
	case EActMotionRotationSourceType::Actor:
		{
			AActor* TargetActor = State.Params.Rotation.TargetActor.Get();
			if (!TargetActor || !CharacterOwner)
			{
				return FVector::ZeroVector;
			}

			switch (State.Params.Rotation.ActorMode)
			{
			case EActMotionRotationActorMode::MatchTargetForward:
				DesiredDirection = TargetActor->GetActorForwardVector();
				break;
			case EActMotionRotationActorMode::MatchOppositeTargetForward:
				DesiredDirection = -TargetActor->GetActorForwardVector();
				break;
			case EActMotionRotationActorMode::FaceTarget:
				DesiredDirection = TargetActor->GetActorLocation() - CharacterOwner->GetActorLocation();
				break;
			case EActMotionRotationActorMode::BackToTarget:
				DesiredDirection = CharacterOwner->GetActorLocation() - TargetActor->GetActorLocation();
				break;
			default:
				break;
			}
		}
		break;
	case EActMotionRotationSourceType::Direction:
		DesiredDirection = State.Params.Rotation.Direction;
		break;
	default:
		break;
	}

	DesiredDirection.Z = 0.0f;
	return DesiredDirection.GetSafeNormal();
}

void UActCharacterMovementComponent::ApplyReplicatedMotionCatchUp(FActMotionState& State, const float PreviousElapsedTime, const float NewElapsedTime)
{
	if (!UpdatedComponent || NewElapsedTime <= PreviousElapsedTime + UE_SMALL_NUMBER)
	{
		return;
	}

	const float CatchUpDeltaTime = NewElapsedTime - PreviousElapsedTime;
	FQuat RootMotionRotationDelta = FQuat::Identity;
	FVector CatchUpDisplacement = ResolveMotionDisplacement(State, PreviousElapsedTime, NewElapsedTime, CatchUpDeltaTime, RootMotionRotationDelta);
	if (State.bHasStartLocation && PreviousElapsedTime <= UE_SMALL_NUMBER)
	{
		FQuat TotalRootMotionRotationDelta = FQuat::Identity;
		const FVector TotalAuthoredDisplacement = ResolveMotionDisplacement(State, 0.0f, NewElapsedTime, NewElapsedTime, TotalRootMotionRotationDelta);
		const FVector TotalLaunchDisplacement = ResolveMotionLaunchDisplacement(State, NewElapsedTime);
		const FVector AuthoritativeLocation = State.StartLocation + TotalAuthoredDisplacement + TotalLaunchDisplacement;
		CatchUpDisplacement = AuthoritativeLocation - UpdatedComponent->GetComponentLocation();
		RootMotionRotationDelta = TotalRootMotionRotationDelta;
	}

	FRotator CatchUpRotation = UpdatedComponent->GetComponentRotation();
	bool bHasRotationChange = false;
	if (!RootMotionRotationDelta.Equals(FQuat::Identity))
	{
		CatchUpRotation = (RootMotionRotationDelta * UpdatedComponent->GetComponentQuat()).Rotator();
		bHasRotationChange = true;
	}

	if (CatchUpDisplacement.IsNearlyZero() && !bHasRotationChange)
	{
		return;
	}

	FHitResult Hit;
	bApplyingMotion = true;
	SafeMoveUpdatedComponent(CatchUpDisplacement, CatchUpRotation, true, Hit);
	bApplyingMotion = false;
	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(CatchUpDisplacement, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}
}

void UActCharacterMovementComponent::ApplyAdditiveRotationToActiveMotionBases(const int32 RotationHandle, const FQuat& RotationDelta)
{
	if (RotationHandle == INDEX_NONE || RotationDelta.Equals(FQuat::Identity))
	{
		return;
	}

	bool bUpdatedAnyBasis = false;
	bool bUpdatedReplicatedBasis = false;
	for (TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		FActMotionState& State = Pair.Value;
		if (!DoesMotionPassModeFilter(State.Params) || !State.Params.HasTranslation())
		{
			continue;
		}

		if (State.Params.BasisMode != EActMotionBasisMode::ActorStartFrozen &&
			State.Params.BasisMode != EActMotionBasisMode::MeshStartFrozen)
		{
			continue;
		}

		if (!State.bHasFrozenBasisRotation)
		{
			continue;
		}

		FRotator UpdatedBasisRotation = (RotationDelta * State.FrozenBasisRotation.Quaternion()).Rotator();
		UpdatedBasisRotation.Pitch = 0.0f;
		UpdatedBasisRotation.Roll = 0.0f;
		State.FrozenBasisRotation = UpdatedBasisRotation;
		bUpdatedAnyBasis = true;
		bUpdatedReplicatedBasis |= State.Params.IsNetworkSynchronized() && State.Params.Provenance != EActMotionProvenance::ReplicatedExternal;
	}

	if (bUpdatedAnyBasis)
	{
		++MotionStateRevision;
		if (bUpdatedReplicatedBasis)
		{
			RequestReplicatedMotionRefresh();
		}
	}
}

void UActCharacterMovementComponent::ApplyAbsoluteRotationToActiveMotionBases(const int32 RotationHandle, const FRotator& NewBasisRotation)
{
	if (RotationHandle == INDEX_NONE)
	{
		return;
	}

	bool bUpdatedAnyBasis = false;
	bool bUpdatedReplicatedBasis = false;
	const FRotator SanitizedBasisRotation(0.0f, NewBasisRotation.Yaw, 0.0f);

	for (TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		FActMotionState& State = Pair.Value;
		if (!DoesMotionPassModeFilter(State.Params) || !State.Params.HasTranslation())
		{
			continue;
		}

		if (State.Params.BasisMode != EActMotionBasisMode::ActorStartFrozen &&
			State.Params.BasisMode != EActMotionBasisMode::MeshStartFrozen)
		{
			continue;
		}

		if (!State.bHasFrozenBasisRotation || State.FrozenBasisRotation.Equals(SanitizedBasisRotation, 0.01f))
		{
			continue;
		}

		State.FrozenBasisRotation = SanitizedBasisRotation;
		bUpdatedAnyBasis = true;
		bUpdatedReplicatedBasis |= IsReplicatedMotionSource(State.Params);
	}

	if (bUpdatedAnyBasis)
	{
		++MotionStateRevision;
		if (bUpdatedReplicatedBasis)
		{
			RequestReplicatedMotionRefresh();
		}
	}
}

void UActCharacterMovementComponent::CaptureRuntimeBasis(FActMotionState& State)
{
	switch (State.Params.BasisMode)
	{
	case EActMotionBasisMode::ActorStartFrozen:
		if (UpdatedComponent)
		{
			State.FrozenBasisRotation = FRotator(0.0f, UpdatedComponent->GetComponentRotation().Yaw, 0.0f);
			State.bHasFrozenBasisRotation = true;
		}
		break;
	case EActMotionBasisMode::MeshStartFrozen:
		if (USkeletalMeshComponent* Mesh = State.BasisMesh.Get(); !Mesh && CharacterOwner)
		{
			Mesh = CharacterOwner->GetMesh();
			if (Mesh)
			{
				State.FrozenBasisRotation = FRotator(0.0f, Mesh->GetComponentRotation().Yaw, 0.0f);
				State.bHasFrozenBasisRotation = true;
			}
		}
		else if (Mesh)
		{
			State.FrozenBasisRotation = FRotator(0.0f, Mesh->GetComponentRotation().Yaw, 0.0f);
			State.bHasFrozenBasisRotation = true;
		}
		else if (UpdatedComponent)
		{
			State.FrozenBasisRotation = FRotator(0.0f, UpdatedComponent->GetComponentRotation().Yaw, 0.0f);
			State.bHasFrozenBasisRotation = true;
		}
		break;
	default:
		break;
	}

	if (State.Params.Rotation.bFreezeDirectionAtStart)
	{
		State.FrozenFacingDirection = ResolveMotionFacingDirection(State);
		State.bHasFrozenFacingDirection = !State.FrozenFacingDirection.IsNearlyZero();
	}
}

int32 UActCharacterMovementComponent::AcquireMotionHandle(const int32 ExistingHandle, const int32 SyncId)
{
	if (ExistingHandle != INDEX_NONE)
	{
		return ExistingHandle;
	}

	if (SyncId != INDEX_NONE)
	{
		if (const int32* ExistingSyncHandle = MotionStateMapBySyncId.Find(SyncId))
		{
			return *ExistingSyncHandle;
		}
	}

	return NextMotionHandle++;
}

void UActCharacterMovementComponent::ApplyPendingMotion(const float DeltaSeconds)
{
	if (bApplyingMotion || !UpdatedComponent || MotionStateMap.IsEmpty() || DeltaSeconds <= UE_SMALL_NUMBER)
	{
		PendingMotionDisplacement = FVector::ZeroVector;
		PendingMotionRotation = FQuat::Identity;
		return;
	}

	RefreshActiveRotationTask();
	FActResolvedMotionRotation ResolvedRotation;
	const FVector MotionDisplacement = ConsumeMotionDisplacement(DeltaSeconds, ResolvedRotation);

	FRotator FinalRotation = UpdatedComponent->GetComponentRotation();
	bool bHasRotationChange = false;

	if (TickActiveRotationTask(DeltaSeconds, FinalRotation))
	{
		bHasRotationChange = true;
	}
	else if (ResolvedRotation.bHasRootMotionRotation)
	{
		FinalRotation = (ResolvedRotation.RootMotionDelta * UpdatedComponent->GetComponentQuat()).Rotator();
		bHasRotationChange = !ResolvedRotation.RootMotionDelta.Equals(FQuat::Identity);
	}

	PendingMotionDisplacement = MotionDisplacement;
	PendingMotionRotation = FinalRotation.Quaternion();

	if (PendingMotionDisplacement.IsNearlyZero() && !bHasRotationChange)
	{
		PendingMotionDisplacement = FVector::ZeroVector;
		PendingMotionRotation = FQuat::Identity;
		return;
	}

	FHitResult Hit;
	bApplyingMotion = true;
	SafeMoveUpdatedComponent(PendingMotionDisplacement, FinalRotation, true, Hit);
	bApplyingMotion = false;
	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(PendingMotionDisplacement, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	PendingMotionDisplacement = FVector::ZeroVector;
	PendingMotionRotation = FQuat::Identity;
}

void UActCharacterMovementComponent::RemoveMotionEntries(const TArray<int32>& HandlesToRemove)
{
	if (HandlesToRemove.IsEmpty())
	{
		return;
	}

	bool bRemovedAny = false;
	bool bNeedsPredictionRevision = false;
	bool bNeedsReplicationRefresh = false;
	for (const int32 Handle : HandlesToRemove)
	{
		if (Handle == INDEX_NONE)
		{
			continue;
		}

		if (const FActMotionState* State = MotionStateMap.Find(Handle); State && State->Params.SyncId != INDEX_NONE)
		{
			ConfirmedOwnerPredictedMotionSyncIds.Remove(State->Params.SyncId);
		}

		const FActMotionState* State = MotionStateMap.Find(Handle);
		if (State)
		{
			bNeedsPredictionRevision |= ShouldAffectPredictionRevision(State->Params);
			bNeedsReplicationRefresh |= IsReplicatedMotionSource(State->Params);
		}
		RemoveMotionMappings(Handle, State);
		bRemovedAny |= MotionStateMap.Remove(Handle) > 0;
		if (Handle == ActiveRotationHandle)
		{
			ActiveRotationHandle = INDEX_NONE;
		}
		if (Handle == LockOnRotationHandle)
		{
			LockOnRotationHandle = INDEX_NONE;
		}
	}

	if (!bRemovedAny)
	{
		return;
	}

	if (bNeedsPredictionRevision)
	{
		++MotionStateRevision;
	}
	RefreshNetworkCorrectionMode();
	RefreshActiveRotationTask();
	if (bNeedsReplicationRefresh)
	{
		RequestReplicatedMotionRefresh();
	}
}

void UActCharacterMovementComponent::RemoveMotionMappings(const int32 Handle, const FActMotionState* State)
{
	if (!State)
	{
		return;
	}

	if (State->Params.SyncId != INDEX_NONE)
	{
		if (const int32* SyncHandle = MotionStateMapBySyncId.Find(State->Params.SyncId); SyncHandle && *SyncHandle == Handle)
		{
			MotionStateMapBySyncId.Remove(State->Params.SyncId);
		}
	}
}

void UActCharacterMovementComponent::RefreshNetworkCorrectionMode()
{
	bool bShouldIgnoreClientErrorCorrection = false;
	bool bShouldAcceptClientAuthoritativePosition = false;
	bool bHasExternalLaunchCorrectionGrace = false;

	if (ExternalLaunchCorrectionGraceEndTime > 0.0f)
	{
		if (const UWorld* World = GetWorld())
		{
			const bool bGraceExpired = World->GetTimeSeconds() >= ExternalLaunchCorrectionGraceEndTime;
			if (!bGraceExpired && MovementMode == MOVE_Falling)
			{
				bHasExternalLaunchCorrectionGrace = true;
			}
			else
			{
				ExternalLaunchCorrectionGraceEndTime = 0.0f;
			}
		}
		else
		{
			ExternalLaunchCorrectionGraceEndTime = 0.0f;
		}
	}

	if (CharacterOwner)
	{
		const bool bIsLocallyControlled = CharacterOwner->IsLocallyControlled();
		const bool bIsAuthorityRemoteAutonomous = CharacterOwner->HasAuthority() && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy;
		bool bHasOwnerPredictedMotion = false;
		bool bHasExternalMotion = false;
		for (const TPair<int32, FActMotionState>& Pair : MotionStateMap)
		{
			const EActMotionProvenance Provenance = Pair.Value.Params.Provenance;
			if (Provenance == EActMotionProvenance::OwnerPredicted)
			{
				bHasOwnerPredictedMotion = true;
			}
			else if (Provenance == EActMotionProvenance::AuthorityExternal || Provenance == EActMotionProvenance::ReplicatedExternal)
			{
				bHasExternalMotion = true;
			}
		}

		if (bIsLocallyControlled && (bHasOwnerPredictedMotion || bHasExternalMotion || bHasExternalLaunchCorrectionGrace))
		{
			bShouldIgnoreClientErrorCorrection = true;
			bShouldAcceptClientAuthoritativePosition = true;
		}

		if (bIsAuthorityRemoteAutonomous)
		{
			if (bHasExternalMotion || bHasExternalLaunchCorrectionGrace)
			{
				// External motion must remain server-authoritative for remote autonomous pawns.
				// Otherwise a client-owned victim can immediately overwrite the server-applied launch/knockback,
				// which makes simulated proxies see only a tiny hop instead of the full hit reaction.
				bShouldIgnoreClientErrorCorrection = true;
				bShouldAcceptClientAuthoritativePosition = false;
			}
			else if (bHasOwnerPredictedMotion)
			{
				bShouldIgnoreClientErrorCorrection = true;
				bShouldAcceptClientAuthoritativePosition = true;
			}
		}
	}

	if (bShouldIgnoreClientErrorCorrection == bMotionNetworkCorrectionOverrideActive &&
		(!bMotionNetworkCorrectionOverrideActive || bShouldAcceptClientAuthoritativePosition == bServerAcceptClientAuthoritativePosition))
	{
		return;
	}

	if (bShouldIgnoreClientErrorCorrection)
	{
		if (!bMotionNetworkCorrectionOverrideActive)
		{
			bCachedIgnoreClientMovementErrorChecksAndCorrection = bIgnoreClientMovementErrorChecksAndCorrection;
			bCachedServerAcceptClientAuthoritativePosition = bServerAcceptClientAuthoritativePosition;
		}

		bIgnoreClientMovementErrorChecksAndCorrection = true;
		bServerAcceptClientAuthoritativePosition = bShouldAcceptClientAuthoritativePosition;
		bMotionNetworkCorrectionOverrideActive = true;
		return;
	}

	bIgnoreClientMovementErrorChecksAndCorrection = bCachedIgnoreClientMovementErrorChecksAndCorrection;
	bServerAcceptClientAuthoritativePosition = bCachedServerAcceptClientAuthoritativePosition;
	bMotionNetworkCorrectionOverrideActive = false;
}

void UActCharacterMovementComponent::RefreshMovementStateParamsFromStack()
{
	if (MovementStateStack.IsEmpty())
	{
		ResetMovementStateParams();
		return;
	}

	ApplyMovementStateParams(MovementStateStack.Last().Params);
}

float UActCharacterMovementComponent::GetReplicatedServerWorldTimeSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}

		return World->GetTimeSeconds();
	}

	return 0.0f;
}

void UActCharacterMovementComponent::RequestReplicatedMotionRefresh() const
{
	if (AActCharacter* ActCharacter = CharacterOwner ? Cast<AActCharacter>(CharacterOwner) : nullptr; ActCharacter && ActCharacter->HasAuthority())
	{
		ActCharacter->RefreshReplicatedMotionStateFromMovement();
		ActCharacter->ForceNetUpdate();
	}
}

bool UActCharacterMovementComponent::HasBlockingMotionRotation() const
{
	if (const FActMotionState* ActiveState = MotionStateMap.Find(ActiveRotationHandle))
	{
		if (CanStateDriveRotationTask(*ActiveState) && !ActiveState->bRotationPaused)
		{
			return true;
		}
	}

	for (const TPair<int32, FActMotionState>& Pair : MotionStateMap)
	{
		const FActMotionState& State = Pair.Value;
		if (!DoesMotionPassModeFilter(State.Params))
		{
			continue;
		}

		if (State.Params.SourceType == EActMotionSourceType::MontageRootMotion &&
			State.Params.bApplyRootMotionRotation &&
			!State.bRootMotionRotationSuppressed)
		{
			return true;
		}
	}

	return false;
}
