// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Tasks/AT_PlayMontageAndWaitForEvent.h"

#include "AbilitySystemGlobals.h"
#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/ActCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Crc.h"

namespace
{
bool CanApplyLocalMontageMotion(const ACharacter* Character, const UGameplayAbility* Ability)
{
	if (!Character || !Ability)
	{
		return false;
	}

	if (Character->GetLocalRole() == ROLE_Authority)
	{
		return true;
	}

	if (Character->GetLocalRole() != ROLE_AutonomousProxy)
	{
		return false;
	}

	switch (Ability->GetNetExecutionPolicy())
	{
	case EGameplayAbilityNetExecutionPolicy::LocalPredicted:
	case EGameplayAbilityNetExecutionPolicy::ServerInitiated:
		return true;
	default:
		return false;
	}
}

bool ResolveMontageSectionTrackRange(const UAnimMontage* Montage, const int32 SectionIndex, float& OutStartTrackPosition, float& OutEndTrackPosition)
{
	if (!Montage || SectionIndex == INDEX_NONE || !Montage->CompositeSections.IsValidIndex(SectionIndex))
	{
		return false;
	}

	const FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
	const float MontageLength = Montage->GetPlayLength();
	const float SectionStartTrackPosition = FMath::Clamp(Section.GetTime(), 0.0f, MontageLength);

	float SectionEndTrackPosition = MontageLength;
	for (int32 NextSectionIndex = SectionIndex + 1; NextSectionIndex < Montage->CompositeSections.Num(); ++NextSectionIndex)
	{
		const float CandidateStartTrackPosition = FMath::Clamp(Montage->CompositeSections[NextSectionIndex].GetTime(), 0.0f, MontageLength);
		if (CandidateStartTrackPosition > SectionStartTrackPosition + UE_SMALL_NUMBER)
		{
			SectionEndTrackPosition = CandidateStartTrackPosition;
			break;
		}
	}

	if (SectionEndTrackPosition <= SectionStartTrackPosition + UE_SMALL_NUMBER)
	{
		return false;
	}

	OutStartTrackPosition = SectionStartTrackPosition;
	OutEndTrackPosition = SectionEndTrackPosition;
	return true;
}

bool ResolveMontageMotionTrackRange(
	const UAnimMontage* Montage,
	const FName StartSectionName,
	const EActMontageMotionRangeMode RangeMode,
	const FName EndSectionName,
	float& OutStartTrackPosition,
	float& OutEndTrackPosition)
{
	if (!Montage)
	{
		return false;
	}

	const float MontageLength = Montage->GetPlayLength();
	float StartTrackPosition = 0.0f;
	float EndTrackPosition = MontageLength;

	const int32 StartSectionIndex = StartSectionName.IsNone() ? INDEX_NONE : Montage->GetSectionIndex(StartSectionName);
	if (StartSectionIndex != INDEX_NONE)
	{
		if (!ResolveMontageSectionTrackRange(Montage, StartSectionIndex, StartTrackPosition, EndTrackPosition))
		{
			return false;
		}
	}

	switch (RangeMode)
	{
	case EActMontageMotionRangeMode::CurrentSection:
		break;

	case EActMontageMotionRangeMode::ThroughSection:
		if (!EndSectionName.IsNone())
		{
			const int32 EndSectionIndex = Montage->GetSectionIndex(EndSectionName);
			if (EndSectionIndex != INDEX_NONE)
			{
				float SectionStart = 0.0f;
				float SectionEnd = 0.0f;
				if (ResolveMontageSectionTrackRange(Montage, EndSectionIndex, SectionStart, SectionEnd))
				{
					EndTrackPosition = SectionEnd;
				}
			}
		}
		break;

	case EActMontageMotionRangeMode::EntireMontage:
		if (StartSectionIndex == INDEX_NONE)
		{
			StartTrackPosition = 0.0f;
		}
		EndTrackPosition = MontageLength;
		break;
	}

	if (FMath::IsNearlyEqual(StartTrackPosition, EndTrackPosition, UE_SMALL_NUMBER))
	{
		return false;
	}

	OutStartTrackPosition = FMath::Clamp(StartTrackPosition, 0.0f, MontageLength);
	OutEndTrackPosition = FMath::Clamp(EndTrackPosition, 0.0f, MontageLength);
	if (OutEndTrackPosition <= OutStartTrackPosition + UE_SMALL_NUMBER)
	{
		return false;
	}

	return true;
}
}

UAT_PlayMontageAndWaitForEvent::UAT_PlayMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Rate = 1.f;
	bStopWhenAbilityEnds = true;
	bTickingTask = true;
}

UAT_PlayMontageAndWaitForEvent* UAT_PlayMontageAndWaitForEvent::CreatePlayMontageAndWaitForEvent(
	UGameplayAbility* OwningAbility, FName TaskInstanceName, UAnimMontage* MontageToPlay,
	FGameplayTagContainer EventTags, float Rate, FName StartSection, bool bStopWhenAbilityEnds,
	float AnimRootMotionTranslationScale, FActMontageMotionSettings MontageMotionSettings)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

	UAT_PlayMontageAndWaitForEvent* NewTask = NewAbilityTask<UAT_PlayMontageAndWaitForEvent>(OwningAbility, TaskInstanceName);
	NewTask->MontageToPlay = MontageToPlay;
	NewTask->EventTags = EventTags;
	NewTask->Rate = Rate;
	NewTask->StartSection = StartSection;
	NewTask->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	NewTask->bStopWhenAbilityEnds = bStopWhenAbilityEnds;
	NewTask->MontageMotionSettings = MontageMotionSettings;

	return NewTask;
}

void UAT_PlayMontageAndWaitForEvent::Activate()
{
	if (Ability == nullptr)
	{
		return;
	}

	bool bPlayedMontage = false;
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		
		if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
		{
			// Bind to event callback
			EventHandle = ActASC->AddGameplayEventTagContainerDelegate(EventTags, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnGameplayEvent));

			FName EffectiveStartSection = StartSection;

			if (ActASC->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), MontageToPlay, Rate, EffectiveStartSection) > 0.f)
			{
				// Playing a montage could potentially fire off a callback into game code which could kill this ability! Early out if we are  pending kill.
				if (ShouldBroadcastAbilityTaskDelegates() == false)
				{
					return;
				}
				
				CancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &ThisClass::OnAbilityCancelled);

				BlendingOutDelegate.BindUObject(this, &ThisClass::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				MontageEndedDelegate.BindUObject(this, &ThisClass::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

				ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
				const bool bCanApplyLocalMontageMotion = CanApplyLocalMontageMotion(Character, Ability);

				if (MontageMotionSettings.bEnabled && bCanApplyLocalMontageMotion && MontageToPlay && Rate > UE_SMALL_NUMBER)
				{
					RefreshPredictedMotionForSection(EffectiveStartSection);
				}
				else if (bCanApplyLocalMontageMotion)
				{
					Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
				}

				bPlayedMontage = true;
			}
		}
		else
		{
			UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_PlayMontageAndWaitForEvent call to PlayMontage failed!"));
		}
	}
	else
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_PlayMontageAndWaitForEvent called on invalid AbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_PlayMontageAndWaitForEvent called in Ability %s failed to play montage %s; Task Instance Name %s."), *Ability->GetName(), *GetNameSafe(MontageToPlay),*InstanceName.ToString());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	SetWaitingOnAvatar();
}

void UAT_PlayMontageAndWaitForEvent::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!MontageMotionSettings.bEnabled || !MontageToPlay || Rate <= UE_SMALL_NUMBER)
	{
		return;
	}

	const FName CurrentSectionName = GetCurrentMontageSectionName();
	if (CurrentSectionName.IsNone() || CurrentSectionName == LastAppliedMotionSection)
	{
		return;
	}

	RefreshPredictedMotionForSection(CurrentSectionName);
}

void UAT_PlayMontageAndWaitForEvent::ExternalCancel()
{
	check(AbilitySystemComponent.Get());

	OnAbilityCancelled();

	Super::ExternalCancel();
}

FString UAT_PlayMontageAndWaitForEvent::GetDebugString() const
{
	UAnimMontage* PlayingMontage = nullptr;
	if (Ability)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
		{
			PlayingMontage = AnimInstance->Montage_IsActive(MontageToPlay) ? MontageToPlay : AnimInstance->GetCurrentActiveMontage();
		}
	}

	return FString::Printf(TEXT("PlayMontageAndWaitForEvent. MontageToPlay: %s  (Currently Playing): %s"), *GetNameSafe(MontageToPlay), *GetNameSafe(PlayingMontage));
}

void UAT_PlayMontageAndWaitForEvent::OnDestroy(bool bInOwnerFinished)
{
	// Note: Clearing montage end delegate isn't necessary since its not a multicast and will be cleared when the next montage plays.
	// (If we are destroyed, it will detect this and not do anything)

	// This delegate, however, should be cleared as it is a multicast
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(CancelledHandle);
		if (bInOwnerFinished && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}

	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		ActASC->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
	}

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
	{
		if (AddMoveHandle != INDEX_NONE)
		{
			if (UActCharacterMovementComponent* ActMovementComponent = Cast<UActCharacterMovementComponent>(Character->GetCharacterMovement()))
			{
				ActMovementComponent->StopAddMove(AddMoveHandle);
			}
			AddMoveHandle = INDEX_NONE;
		}

		if (bPushedDisableRootMotion)
		{
			if (Ability)
			{
				if (const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo())
				{
					if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
					{
						if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
						{
							MontageInstance->PopDisableRootMotion();
						}
					}
				}
			}

			bPushedDisableRootMotion = false;
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}

bool UAT_PlayMontageAndWaitForEvent::StopPlayingMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	// Check if the montage is still playing
	// The ability would have been interrupted, in which case we should automatically stop the montage
	if (AbilitySystemComponent.IsValid() && Ability)
	{
		if (AbilitySystemComponent->GetAnimatingAbility() == Ability && AbilitySystemComponent->GetCurrentMontage() == MontageToPlay)
		{
			// Unbind delegates so they don't get called as well
			if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
			{
				MontageInstance->OnMontageBlendingOutStarted.Unbind();
				MontageInstance->OnMontageEnded.Unbind();
			}

			AbilitySystemComponent->CurrentMontageStop();
			
			return true;
		}
	}

	return false;
}

UActAbilitySystemComponent* UAT_PlayMontageAndWaitForEvent::GetActAbilitySystemComponent()
{
	return Cast<UActAbilitySystemComponent>(AbilitySystemComponent);
}

bool UAT_PlayMontageAndWaitForEvent::RefreshPredictedMotionForSection(const FName SectionName)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (!Character || !MontageToPlay)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}

	if (!bPushedDisableRootMotion)
	{
		if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
		{
			MontageInstance->PushDisableRootMotion();
			bPushedDisableRootMotion = true;
		}
	}

	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	if (!MovementComponent)
	{
		return false;
	}

	float MotionStartTrackPosition = 0.0f;
	float MotionEndTrackPosition = 0.0f;
	if (!ResolveMontageMotionTrackRange(MontageToPlay, SectionName, MontageMotionSettings.RangeMode, MontageMotionSettings.EndSectionName, MotionStartTrackPosition, MotionEndTrackPosition))
	{
		if (AddMoveHandle != INDEX_NONE)
		{
			if (UActCharacterMovementComponent* ActMovementComponent = Cast<UActCharacterMovementComponent>(MovementComponent))
			{
				ActMovementComponent->StopAddMove(AddMoveHandle);
			}
			AddMoveHandle = INDEX_NONE;
		}

		// Treat degenerate / zero-length sections as "no authored motion in this section" instead of
		// a hard runtime failure. Mark the section as processed so TickTask does not retry every frame.
		LastAppliedMotionSection = SectionName;
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] ExtractedMontageMotion skipped. Ability=%s Montage=%s StartSection=%s"),
			*GetNameSafe(Ability),
			*GetNameSafe(MontageToPlay),
			*SectionName.ToString());
		return false;
	}

	const float ExtractedDuration = FMath::Abs(MotionEndTrackPosition - MotionStartTrackPosition) / FMath::Max(FMath::Abs(Rate), UE_SMALL_NUMBER);
	if (ExtractedDuration <= UE_SMALL_NUMBER)
	{
		LastAppliedMotionSection = SectionName;
		return false;
	}

	if (UActCharacterMovementComponent* ActMovementComponent = Cast<UActCharacterMovementComponent>(MovementComponent))
	{
		if (AddMoveHandle == INDEX_NONE && MontageMotionSettings.bBrakeMovementOnStart)
		{
			// Attack startup should normally override locomotion immediately instead of inheriting
			// pre-attack run speed into the authored forward motion.
			ActMovementComponent->StopMovementImmediately();
		}

		AddMoveHandle = ActMovementComponent->SetAddMoveFromMontage(
			MontageToPlay,
			MotionStartTrackPosition,
			MotionEndTrackPosition,
			ExtractedDuration,
			MontageMotionSettings.bApplyRotation,
			MontageMotionSettings.bIgnoreZAccumulate,
			AddMoveHandle,
			GetOrCreateMotionSyncId());
		LastAppliedMotionSection = SectionName;
		return AddMoveHandle != INDEX_NONE;
	}

	return false;
}

FName UAT_PlayMontageAndWaitForEvent::GetCurrentMontageSectionName() const
{
	if (!Ability || !MontageToPlay)
	{
		return NAME_None;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	return AnimInstance ? AnimInstance->Montage_GetCurrentSection(MontageToPlay) : NAME_None;
}

int32 UAT_PlayMontageAndWaitForEvent::GetOrCreateMotionSyncId() const
{
	if (MotionSyncId != INDEX_NONE)
	{
		return MotionSyncId;
	}

	uint32 Hash = GetTypeHash(GetNameSafe(MontageToPlay));
	Hash = HashCombine(Hash, GetTypeHash(InstanceName));

	if (Ability)
	{
		Hash = HashCombine(Hash, FCrc::StrCrc32(*Ability->GetCurrentAbilitySpecHandle().ToString()));
		Hash = HashCombine(Hash, ::GetTypeHash(Ability->GetCurrentActivationInfo().GetActivationPredictionKey().Current));
	}

	MotionSyncId = static_cast<int32>(Hash & 0x7fffffff);
	if (MotionSyncId == INDEX_NONE)
	{
		MotionSyncId = FCrc::StrCrc32(TEXT("ActMontageMotionSync"));
	}

	return MotionSyncId;
}

void UAT_PlayMontageAndWaitForEvent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Ability && Ability->GetCurrentMontage() == MontageToPlay)
	{
		if (Montage == MontageToPlay)
		{
			AbilitySystemComponent->ClearAnimatingAbility(Ability);

			// Reset AnimRootMotionTranslationScale only when native montage root motion was used.
			ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
			if (!MontageMotionSettings.bEnabled && CanApplyLocalMontageMotion(Character, Ability))
			{
				Character->SetAnimRootMotionTranslationScale(1.f);
			}
		}
	}

	if (bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
		else
		{
			if (ShouldBroadcastAbilityTaskDelegates())
			{
				OnBlendOut.Broadcast(FGameplayTag(), FGameplayEventData());
			}
		}
	}
}

void UAT_PlayMontageAndWaitForEvent::OnAbilityCancelled()
{
	// TODO: Merge this fix back to engine, it was calling the wrong callback

	if (StopPlayingMontage())
	{
		// Let the BP handle the interrupt as well
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
}

void UAT_PlayMontageAndWaitForEvent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	EndTask();
}

void UAT_PlayMontageAndWaitForEvent::OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;

		EventReceived.Broadcast(EventTag, TempData);
	}
}
