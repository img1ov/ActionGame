// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Tasks/AT_ApplySetAddMove.h"

#include "AbilitySystemGlobals.h"
#include "ActLogChannels.h"
#include "Character/ActCharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AT_ApplySetAddMove)

UAT_ApplySetAddMove::UAT_ApplySetAddMove(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAT_ApplySetAddMove* UAT_ApplySetAddMove::ApplySetAddMove(
	UGameplayAbility* OwningAbility,
	const FName TaskInstanceName,
	const EActAddMoveSpace InSpace,
	FVector InDirection,
	const float InStrength,
	float InDuration,
	UCurveFloat* InStrengthOverTime,
	USkeletalMeshComponent* InMesh)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(InDuration);

	UAT_ApplySetAddMove* Task = NewAbilityTask<UAT_ApplySetAddMove>(OwningAbility, TaskInstanceName);
	Task->Space = InSpace;
	Task->Direction = InDirection.GetSafeNormal();
	Task->Strength = InStrength;
	Task->Duration = InDuration;
	Task->StrengthOverTime = InStrengthOverTime;
	Task->Mesh = InMesh;
	return Task;
}

void UAT_ApplySetAddMove::Activate()
{
	if (!ApplyAuthoredAddMove())
	{
		EndTask();
		return;
	}

	SetWaitingOnAvatar();
}

void UAT_ApplySetAddMove::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (HasInfiniteDuration())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		EndTask();
		return;
	}

	if ((World->GetTimeSeconds() - StartTimeSeconds) < Duration)
	{
		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinish.Broadcast();
	}

	EndTask();
}

void UAT_ApplySetAddMove::ExternalCancel()
{
	Super::ExternalCancel();
	EndTask();
}

void UAT_ApplySetAddMove::OnDestroy(const bool bInOwnerFinished)
{
	if (AddMoveHandle != INDEX_NONE)
	{
		if (UActCharacterMovementComponent* MovementComponent = GetActMovementComponent())
		{
			MovementComponent->StopAddMove(AddMoveHandle);
		}

		AddMoveHandle = INDEX_NONE;
	}

	Super::OnDestroy(bInOwnerFinished);
}

FString UAT_ApplySetAddMove::GetDebugString() const
{
	return FString::Printf(
		TEXT("ApplySetAddMove. Space=%d Direction=%s Strength=%.2f Duration=%.2f Handle=%d"),
		static_cast<int32>(Space),
		*Direction.ToString(),
		Strength,
		Duration,
		AddMoveHandle);
}

UActCharacterMovementComponent* UAT_ApplySetAddMove::GetActMovementComponent() const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	return Character ? Cast<UActCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}

USkeletalMeshComponent* UAT_ApplySetAddMove::ResolveMeshBasis() const
{
	if (Mesh)
	{
		return Mesh;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	return Character ? Character->GetMesh() : nullptr;
}

bool UAT_ApplySetAddMove::ApplyAuthoredAddMove()
{
	UActCharacterMovementComponent* MovementComponent = GetActMovementComponent();
	if (!MovementComponent)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_ApplySetAddMove called in Ability %s with null ActCharacterMovementComponent; Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*InstanceName.ToString());
		return false;
	}

	if (Direction.IsNearlyZero() || FMath::IsNearlyZero(Strength) || FMath::IsNearlyZero(Duration))
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_ApplySetAddMove called in Ability %s with invalid movement params. Direction=%s Strength=%.3f Duration=%.3f Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*Direction.ToString(),
			Strength,
			Duration,
			*InstanceName.ToString());
		return false;
	}

	const FVector Velocity = Direction * Strength;
	const EActAddMoveCurveType CurveType = StrengthOverTime ? EActAddMoveCurveType::CustomCurve : EActAddMoveCurveType::Constant;

	switch (Space)
	{
	case EActAddMoveSpace::World:
		AddMoveHandle = MovementComponent->SetAddMoveWorld(Velocity, Duration, CurveType, StrengthOverTime);
		break;

	case EActAddMoveSpace::Actor:
		AddMoveHandle = MovementComponent->SetAddMove(Velocity, Duration, CurveType, StrengthOverTime);
		break;

	case EActAddMoveSpace::Mesh:
		AddMoveHandle = MovementComponent->SetAddMoveWithMesh(ResolveMeshBasis(), Velocity, Duration, CurveType, StrengthOverTime);
		break;

	default:
		AddMoveHandle = INDEX_NONE;
		break;
	}

	if (AddMoveHandle == INDEX_NONE)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_ApplySetAddMove failed to allocate AddMove handle. Ability=%s Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*InstanceName.ToString());
		return false;
	}

	if (const UWorld* World = GetWorld())
	{
		StartTimeSeconds = World->GetTimeSeconds();
	}

	return true;
}

bool UAT_ApplySetAddMove::HasInfiniteDuration() const
{
	return Duration < 0.0f;
}
