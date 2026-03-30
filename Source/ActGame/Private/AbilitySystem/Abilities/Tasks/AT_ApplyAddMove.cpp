// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Tasks/AT_ApplyAddMove.h"

#include "AbilitySystemGlobals.h"
#include "ActLogChannels.h"
#include "Character/ActCharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AT_ApplyAddMove)

UAT_ApplyAddMove::UAT_ApplyAddMove(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAT_ApplyAddMove* UAT_ApplyAddMove::ApplyAddMove(
	UGameplayAbility* OwningAbility,
	const FName TaskInstanceName,
	const EActMotionBasisMode InBasisMode,
	FVector InDirection,
	const float InStrength,
	float InDuration,
	const EActMotionProvenance InProvenance,
	UCurveFloat* InStrengthOverTime,
	USkeletalMeshComponent* InMesh)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(InDuration);

	UAT_ApplyAddMove* Task = NewAbilityTask<UAT_ApplyAddMove>(OwningAbility, TaskInstanceName);
	Task->BasisMode = InBasisMode;
	Task->Direction = InDirection.GetSafeNormal();
	Task->Strength = InStrength;
	Task->Duration = InDuration;
	Task->Provenance = InProvenance;
	Task->StrengthOverTime = InStrengthOverTime;
	Task->Mesh = InMesh;
	return Task;
}

UAT_ApplyAddMove* UAT_ApplyAddMove::ApplyAddMoveVelocity(
	UGameplayAbility* OwningAbility,
	const FName TaskInstanceName,
	const EActMotionBasisMode InBasisMode,
	const FVector InVelocity,
	float InDuration,
	const EActMotionProvenance InProvenance,
	UCurveFloat* InStrengthOverTime,
	USkeletalMeshComponent* InMesh)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(InDuration);

	UAT_ApplyAddMove* Task = NewAbilityTask<UAT_ApplyAddMove>(OwningAbility, TaskInstanceName);
	Task->BasisMode = InBasisMode;
	Task->Direction = InVelocity.GetSafeNormal();
	Task->Strength = InVelocity.Size();
	Task->Duration = InDuration;
	Task->Provenance = InProvenance;
	Task->StrengthOverTime = InStrengthOverTime;
	Task->Mesh = InMesh;
	return Task;
}

void UAT_ApplyAddMove::Activate()
{
	if (!ApplyAuthoredAddMove())
	{
		EndTask();
		return;
	}

	if (!HasInfiniteDuration())
	{
		if (UWorld* World = GetWorld())
		{
			if (Duration <= 0.0f)
			{
				World->GetTimerManager().SetTimerForNextTick(this, &UAT_ApplyAddMove::OnTimeFinish);
			}
			else
			{
				World->GetTimerManager().SetTimer(FinishTimerHandle, this, &UAT_ApplyAddMove::OnTimeFinish, Duration, false);
			}
		}
	}

	SetWaitingOnAvatar();
}

void UAT_ApplyAddMove::ExternalCancel()
{
	Super::ExternalCancel();
	EndTask();
}

void UAT_ApplyAddMove::OnDestroy(const bool bInOwnerFinished)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishTimerHandle);
	}

	if (AddMoveHandle != INDEX_NONE)
	{
		if (UActCharacterMovementComponent* MovementComponent = GetActMovementComponent())
		{
			MovementComponent->StopMotion(AddMoveHandle);
		}

		AddMoveHandle = INDEX_NONE;
	}

	Super::OnDestroy(bInOwnerFinished);
}

FString UAT_ApplyAddMove::GetDebugString() const
{
	return FString::Printf(
		TEXT("ApplyAddMove. BasisMode=%d Direction=%s Strength=%.2f Duration=%.2f Handle=%d"),
		static_cast<int32>(BasisMode),
		*Direction.ToString(),
		Strength,
		Duration,
		AddMoveHandle);
}

UActCharacterMovementComponent* UAT_ApplyAddMove::GetActMovementComponent() const
{
	return UActCharacterMovementComponent::ResolveActMovementComponent(GetAvatarActor());
}

USkeletalMeshComponent* UAT_ApplyAddMove::ResolveMeshBasis() const
{
	if (Mesh)
	{
		return Mesh;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	return Character ? Character->GetMesh() : nullptr;
}

bool UAT_ApplyAddMove::ApplyAuthoredAddMove()
{
	UActCharacterMovementComponent* MovementComponent = GetActMovementComponent();
	if (!MovementComponent)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_ApplyAddMove called in Ability %s with null ActCharacterMovementComponent; Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*InstanceName.ToString());
		return false;
	}

	if (Direction.IsNearlyZero() || FMath::IsNearlyZero(Strength) || FMath::IsNearlyZero(Duration))
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_ApplyAddMove called in Ability %s with invalid movement params. Direction=%s Strength=%.3f Duration=%.3f Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*Direction.ToString(),
			Strength,
			Duration,
			*InstanceName.ToString());
		return false;
	}

	const FVector Velocity = Direction * Strength;
	const EActMotionCurveType CurveType = StrengthOverTime ? EActMotionCurveType::CustomCurve : EActMotionCurveType::Constant;
	const int32 SyncId = ShouldUseNetworkSyncId() ? GetOrCreateAddMoveSyncId() : INDEX_NONE;

	FActMotionParams Params;
	Params.SourceType = EActMotionSourceType::Velocity;
	Params.BasisMode = BasisMode;
	Params.Velocity = Velocity;
	Params.Duration = Duration;
	Params.CurveType = CurveType;
	Params.Curve = StrengthOverTime;
	Params.Provenance = Provenance;
	Params.SyncId = SyncId;
	AddMoveHandle = MovementComponent->ApplyMotion(Params, ResolveMeshBasis(), INDEX_NONE);

	if (AddMoveHandle == INDEX_NONE)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_ApplyAddMove failed to allocate movement handle. Ability=%s Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*InstanceName.ToString());
		return false;
	}

	return true;
}

bool UAT_ApplyAddMove::HasInfiniteDuration() const
{
	return Duration < 0.0f;
}

void UAT_ApplyAddMove::OnTimeFinish()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinish.Broadcast();
	}

	EndTask();
}

bool UAT_ApplyAddMove::ShouldUseNetworkSyncId() const
{
	return UActCharacterMovementComponent::ShouldUseMotionNetworkSync(Ability, Provenance);
}

int32 UAT_ApplyAddMove::GetOrCreateAddMoveSyncId() const
{
	if (AddMoveSyncId != INDEX_NONE)
	{
		return AddMoveSyncId;
	}

	AddMoveSyncId = UActCharacterMovementComponent::BuildStableMotionSyncId(InstanceName, Ability, TEXT("ApplyAddMoveSync"));
	return AddMoveSyncId;
}
