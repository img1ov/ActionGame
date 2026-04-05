// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ActBattleComponent.h"

#include "ActGameplayTags.h"
#include "ActLogChannels.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ActCharacterMovementComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/CollisionProfile.h"
#include "CollisionShape.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagAssetInterface.h"
#include "Character/ActCharacter.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActBattleComponent)

UActBattleComponent::UActBattleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);

	TargetCollisionChannel = ECC_Pawn;
	LockOnRange = 1500.0f;
	LockOnRefreshInterval = 0.15f;
	PrimaryComponentTick.TickInterval = LockOnRefreshInterval;
}

void UActBattleComponent::InitializeWithAbilitySystem(UActAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);
	
	if (AbilitySystemComponent)
	{
		UE_LOG(LogAct, Error, TEXT("UActBattleComponent: Battle component for owner [%s] has already been initialized with an ability system."), *GetNameSafe(Owner));
		return;
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogAct, Error, TEXT("UActBattleComponent: Cannot initialize battle component for owner [%s] with NULL ability system."), *GetNameSafe(Owner));
		return;
	}
	
	AbilitySystemComponent->RegisterGameplayTagEvent(ActGameplayTags::Status_Floating, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::OnFloatingTagChanged);
}

void UActBattleComponent::UninitializeFromAbilitySystem()
{
	AbilitySystemComponent = nullptr;
}

void UActBattleComponent::OnFloatingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const AActCharacter* ActCharacter = Cast<AActCharacter>(GetOwner());
	if (!ActCharacter) return;
	
	if (NewCount > 0)
	{
		ActCharacter->GetActMovementComponent()->StartFloating();
	}
	else
	{
		ActCharacter->GetActMovementComponent()->StopFloating();
	}
}

void UActBattleComponent::OnRegister()
{
	Super::OnRegister();

	const APawn* Pawn = GetPawn<APawn>();
	ensureAlwaysMsgf((Pawn != nullptr), TEXT("ActBattleComponent on [%s] can only be added to Pawn actors."), *GetNameSafe(GetOwner()));
	UpdateLockOnTickState();
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

	SetLockOnRequested(true);
	RefreshLockOnTarget();

	if (!Pawn->HasAuthority() && Pawn->IsLocallyControlled())
	{
		ServerSetLockOnRequested(true);
	}
}

void UActBattleComponent::StopLockOnTarget()
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	SetLockOnRequested(false);
	SetLockOnTarget(nullptr);

	if (!Pawn->HasAuthority() && Pawn->IsLocallyControlled())
	{
		ServerSetLockOnRequested(false);
	}
}

void UActBattleComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bLockOnRequested)
	{
		return;
	}

	RefreshLockOnTarget();
}

bool UActBattleComponent::ShouldEvaluateLockOn() const
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return false;
	}

	return Pawn->HasAuthority() || Pawn->IsLocallyControlled() || Pawn->IsBotControlled();
}

void UActBattleComponent::UpdateLockOnTickState()
{
	SetComponentTickEnabled(bLockOnRequested && ShouldEvaluateLockOn());
}

void UActBattleComponent::RefreshLockOnTarget()
{
	if (!bLockOnRequested || !ShouldEvaluateLockOn())
	{
		return;
	}

	// Lock-on is long-lived combat state. Keep the current target while valid, only reacquiring
	// when it is missing or no longer usable.
	AActor* NewTarget = CurrentLockOnTarget;
	if (ShouldClearLockOnTarget(NewTarget))
	{
		NewTarget = nullptr;
	}

	if (!NewTarget)
	{
		NewTarget = FindLockOnTarget();
	}

	SetLockOnTarget(NewTarget);
}

void UActBattleComponent::SetLockOnRequested(const bool bRequested)
{
	if (bLockOnRequested == bRequested)
	{
		return;
	}

	bLockOnRequested = bRequested;
	UpdateLockOnTickState();
	NotifyLockOnStrafeState(bRequested);
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

void UActBattleComponent::NotifyLockOnStrafeState(const bool bRequested)
{
	// Lock-on only toggles facing behavior on the movement component; input/movement direction stays controller-driven.
	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (UActCharacterMovementComponent* ActMoveComp = UActCharacterMovementComponent::ResolveActMovementComponent(Pawn))
		{
			ActMoveComp->SetLockOnStrafeActive(bRequested);
		}
	}
}

void UActBattleComponent::ServerSetLockOnRequested_Implementation(const bool bRequested)
{
	SetLockOnRequested(bRequested);
	if (!bLockOnRequested)
	{
		SetLockOnTarget(nullptr);
		return;
	}

	RefreshLockOnTarget();
}

void UActBattleComponent::OnRep_LockOnTarget(AActor* OldTarget)
{
}

AActor* UActBattleComponent::GetLockOnTarget() const
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
	if (!Pawn)
	{
		return nullptr;
	}

	const bool bUseCameraTrace = Pawn->IsLocallyControlled() && !Pawn->IsBotControlled();
	if (!bUseCameraTrace)
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
