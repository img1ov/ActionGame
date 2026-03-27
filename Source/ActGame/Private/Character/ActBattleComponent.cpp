// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ActBattleComponent.h"

#include "ActGameplayTags.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Engine/CollisionProfile.h"
#include "CollisionShape.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagAssetInterface.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActBattleComponent)

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Movement_StrafeMove, "Event.Movement.StrafeMove", "Strafe-move state (e.g. lock-on hold).");

UActBattleComponent::UActBattleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	TargetCollisionChannel = ECC_Pawn;
	LockOnRange = 1500.0f;
}

void UActBattleComponent::OnRegister()
{
	Super::OnRegister();

	const APawn* Pawn = GetPawn<APawn>();
	ensureAlwaysMsgf((Pawn != nullptr), TEXT("ActBattleComponent on [%s] can only be added to Pawn actors."), *GetNameSafe(GetOwner()));
}

void UActBattleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UActBattleComponent, CurrentLockOnTarget);
}

void UActBattleComponent::StartLockOnTarget()
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	// Authority owns the final target selection, but local control can set immediately for responsiveness.
	const bool bIsAuthority = Pawn->HasAuthority();
	const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
	const bool bIsBotControlled = Pawn->IsBotControlled();
	// Remote pawns on the server should not clear/select; wait for the client's RPC.
	if (bIsAuthority && !bIsLocallyControlled && !bIsBotControlled)
	{
		return;
	}

	AActor* NewTarget = nullptr;
	if (bIsLocallyControlled || bIsBotControlled)
	{
		NewTarget = FindLockOnTarget();
	}

	// Reacquisition should be non-destructive. If the current target is still valid and we fail to
	// find a better candidate this frame, keep the existing lock instead of momentarily clearing it.
	if (!NewTarget && !ShouldClearLockOnTarget(CurrentLockOnTarget))
	{
		NewTarget = CurrentLockOnTarget;
	}

	if (bIsAuthority)
	{
		SetLockOnTarget(NewTarget);
	}
	else if (bIsLocallyControlled)
	{
		SetLockOnTarget(NewTarget);
		ServerSetLockOnTarget(NewTarget);
	}
}

void UActBattleComponent::StopLockOnTarget()
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	if (Pawn->HasAuthority())
	{
		SetLockOnTarget(nullptr);
	}
	else if (Pawn->IsLocallyControlled())
	{
		SetLockOnTarget(nullptr);
		ServerSetLockOnTarget(nullptr);
	}
}

void UActBattleComponent::ServerSetLockOnTarget_Implementation(AActor* NewTarget)
{
	if (!NewTarget)
	{
		SetLockOnTarget(nullptr);
		return;
	}

	if (!IsValidBattleTarget(NewTarget))
	{
		NewTarget = nullptr;
	}

	if (NewTarget && !IsTargetInRange(NewTarget, LockOnRange))
	{
		NewTarget = nullptr;
	}

	SetLockOnTarget(NewTarget);
}

void UActBattleComponent::SetLockOnTarget(AActor* NewTarget)
{
	if (CurrentLockOnTarget == NewTarget)
	{
		return;
	}

	CurrentLockOnTarget = NewTarget;

	// Ensure target changes are replicated promptly on authority.
	if (APawn* PawnToUpdate = GetPawn<APawn>())
	{
		if (PawnToUpdate->HasAuthority())
		{
			PawnToUpdate->ForceNetUpdate();
		}
	}
}

void UActBattleComponent::OnRep_LockOnTarget(AActor* OldTarget)
{
}

AActor* UActBattleComponent::GetLockOnTarget()
{
	return ShouldClearLockOnTarget(CurrentLockOnTarget) ? nullptr : CurrentLockOnTarget;
}

AActor* UActBattleComponent::FindLockOnTarget() const
{
	if (AActor* ByCamera = FindLockOnTargetByCameraTrace())
	{
		return ByCamera;
	}

	return FindLockOnTargetByProximity();
}

AActor* UActBattleComponent::FindLockOnTargetByCameraTrace() const
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return nullptr;
	}

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return nullptr;
	}

	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
	const FVector TraceStart = CameraLocation;
	const FVector TraceEnd = TraceStart + (CameraRotation.Vector() * LockOnRange);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ActBattleComponent_LockOnCameraTrace), false, Pawn);
	FCollisionObjectQueryParams ObjectParams(TargetCollisionChannel);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, Params))
	{
		if (IsValidBattleTarget(Hit.GetActor()))
		{
			return Hit.GetActor();
		}
	}

	return nullptr;
}

AActor* UActBattleComponent::FindLockOnTargetByProximity() const
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return nullptr;
	}

	const FVector Center = Pawn->GetActorLocation();
	const float Radius = LockOnRange;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ActBattleComponent_LockOnOverlap), false, Pawn);
	FCollisionObjectQueryParams ObjectParams(TargetCollisionChannel);

	TArray<FOverlapResult> Overlaps;
	if (!GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, Sphere, Params))
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();

	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Candidate = Result.GetActor();
		if (!IsValidBattleTarget(Candidate))
		{
			continue;
		}

		const double DistSq = FVector::DistSquared(Center, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

bool UActBattleComponent::IsValidBattleTarget(const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	if (Target == GetOwner())
	{
		return false;
	}

	if (const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Target))
	{
		if (TagInterface->HasMatchingGameplayTag(ActGameplayTags::Status_Death_Dead) ||
			TagInterface->HasMatchingGameplayTag(ActGameplayTags::Status_Death_Dying))
		{
			return false;
		}
	}

	return true;
}

bool UActBattleComponent::IsTargetInRange(const AActor* Target, float MaxDistance) const
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn || !Target)
	{
		return false;
	}

	return FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(MaxDistance);
}

bool UActBattleComponent::ShouldClearLockOnTarget(const AActor* Target) const
{
	if (!IsValidBattleTarget(Target))
	{
		return true;
	}

	return !IsTargetInRange(Target, LockOnRange);
}
