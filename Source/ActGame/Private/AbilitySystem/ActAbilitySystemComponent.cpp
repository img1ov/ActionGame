// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/ActAbilitySystemComponent.h"

#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilityTagRelationshipMapping.h"
#include "AbilitySystem/Abilities/ActGameplayAbility.h"
#include "Animation/ActAnimInstance.h"
#include "Blueprint/BulletBlueprintLibrary.h"
#include "Character/ActPawnData.h"
#include "Player/ActPlayerState.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Gameplay_AbilityInputBlocked, "Gameplay.AbilityInputBlocked");

UActAbilitySystemComponent::UActAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UActAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	check(ActorInfo);
	check(InOwnerActor);
	
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (UActAnimInstance* ActAnimInst = Cast<UActAnimInstance>(ActorInfo->GetAnimInstance()))
	{
		ActAnimInst->InitializeWithAbilitySystem(this);
	}
}

void UActAbilitySystemComponent::CancelAbilitiesByFunc(const TShouldCancelAbilityFunc& ShouldCancelFunc, const bool bReplicateCancelAbility)
{
	ABILITYLIST_SCOPE_LOCK();

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.IsActive())
		{
			continue;
		}

		UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec.Ability);
		if (!ActAbilityCDO)
		{
			UE_LOG(LogActAbilitySystem, Error, TEXT("CancelAbilitiesByFunc: Non-UActGameplayAbility %s was Granted to ASC. Skipping."), *AbilitySpec.Ability.GetName());
			continue;
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ensureMsgf(AbilitySpec.Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced, TEXT("CancelAbilitiesByFunc: All Abilities should be Instanced (NonInstanced is being deprecated due to usability issues)."));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// Cancel all the spawned instances.
		TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
		for (UGameplayAbility* AbilityInstance : Instances)
		{
			UActGameplayAbility* ActAbilityInstance = CastChecked<UActGameplayAbility>(AbilityInstance);

			if (ShouldCancelFunc(ActAbilityInstance, AbilitySpec.Handle))
			{
				if (ActAbilityInstance->CanBeCanceled())
				{
					ActAbilityInstance->CancelAbility(AbilitySpec.Handle, AbilityActorInfo.Get(), ActAbilityInstance->GetCurrentActivationInfo(), bReplicateCancelAbility);
				}
				else
				{
					UE_LOG(LogActAbilitySystem, Error, TEXT("CancelAbilitiesByFunc: Can't cancel ability [%s] because CanBeCanceled is false."), *ActAbilityInstance->GetName());
				}
			}
		}
	}
}

void UActAbilitySystemComponent::CancelInputActivatedAbilities(bool bReplicateCancelAbility)
{
	auto ShouldCancelFunc = [this](const UActGameplayAbility* ActAbility, FGameplayAbilitySpecHandle Handle)
	{
		const EActAbilityActivationPolicy ActivationPolicy = ActAbility->GetActivationPolicy();
		return ((ActivationPolicy == EActAbilityActivationPolicy::OnInputTriggered) || (ActivationPolicy == EActAbilityActivationPolicy::WhileInputActive));
	};

	CancelAbilitiesByFunc(ShouldCancelFunc, bReplicateCancelAbility);
}

void UActAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void UActAbilitySystemComponent::AbilityInputTagTriggered(const FGameplayTag InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				// One-shot trigger for command outputs: do not add to held handles.
				InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void UActAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.Remove(AbilitySpec.Handle);
			}
		}
	}
}

bool UActAbilitySystemComponent::TryActivateGrantedAbilityByClass(
	const TSubclassOf<UActGameplayAbility> AbilityClass,
	const bool bAllowRemoteActivation,
	const bool bCancelIfAlreadyActive,
	TSubclassOf<UActGameplayAbility>* OutActivatedAbilityClass,
	FGameplayAbilitySpecHandle* OutActivatedSpecHandle)
{
	if (!AbilityClass)
	{
		return false;
	}

	ABILITYLIST_SCOPE_LOCK();

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability)
		{
			continue;
		}

		if (!AbilitySpec.Ability->GetClass()->IsChildOf(AbilityClass))
		{
			continue;
		}

		if (bCancelIfAlreadyActive && AbilitySpec.IsActive())
		{
			CancelAbilityHandle(AbilitySpec.Handle);
		}

		const bool bRequested = TryActivateAbility(AbilitySpec.Handle, bAllowRemoteActivation);
		if (!bRequested)
		{
			return false;
		}

		if (const FGameplayAbilitySpec* ActivatedSpec = FindAbilitySpecFromHandle(AbilitySpec.Handle))
		{
			if (ActivatedSpec->IsActive())
			{
				if (OutActivatedAbilityClass)
				{
					*OutActivatedAbilityClass = AbilitySpec.Ability->GetClass();
				}
				if (OutActivatedSpecHandle)
				{
					*OutActivatedSpecHandle = AbilitySpec.Handle;
				}
				return true;
			}
		}

		return false;
	}

	return false;
}

bool UActAbilitySystemComponent::TryActivateGrantedAbilityByInputTag(
	const FGameplayTag InputTag,
	const bool bAllowRemoteActivation,
	const bool bCancelIfAlreadyActive,
	TSubclassOf<UActGameplayAbility>* OutActivatedAbilityClass,
	FGameplayAbilitySpecHandle* OutActivatedSpecHandle)
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	ABILITYLIST_SCOPE_LOCK();

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		if (bCancelIfAlreadyActive && AbilitySpec.IsActive())
		{
			CancelAbilityHandle(AbilitySpec.Handle);
		}

		const bool bRequested = TryActivateAbility(AbilitySpec.Handle, bAllowRemoteActivation);
		if (!bRequested)
		{
			return false;
		}

		if (const FGameplayAbilitySpec* ActivatedSpec = FindAbilitySpecFromHandle(AbilitySpec.Handle))
		{
			if (ActivatedSpec->IsActive())
			{
				if (OutActivatedAbilityClass)
				{
					*OutActivatedAbilityClass = AbilitySpec.Ability->GetClass();
				}
				if (OutActivatedSpecHandle)
				{
					*OutActivatedSpecHandle = AbilitySpec.Handle;
				}
				return true;
			}
		}

		return false;
	}

	return false;
}

bool UActAbilitySystemComponent::HasGrantedAbilityByClass(const TSubclassOf<UActGameplayAbility> AbilityClass)
{
	if (!AbilityClass)
	{
		return false;
	}

	ABILITYLIST_SCOPE_LOCK();

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (AbilitySpec.Ability && AbilitySpec.Ability->GetClass()->IsChildOf(AbilityClass))
		{
			return true;
		}
	}

	return false;
}

bool UActAbilitySystemComponent::FindActiveAbilityInstanceByClass(
	const TSubclassOf<UActGameplayAbility> AbilityClass,
	TWeakObjectPtr<UActGameplayAbility>& OutAbilityInstance)
{
	OutAbilityInstance.Reset();

	if (!AbilityClass)
	{
		return false;
	}

	ABILITYLIST_SCOPE_LOCK();

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.IsActive() || !AbilitySpec.Ability || !AbilitySpec.Ability->GetClass()->IsChildOf(AbilityClass))
		{
			continue;
		}

		const TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
		for (UGameplayAbility* AbilityInstance : Instances)
		{
			if (UActGameplayAbility* ActAbility = Cast<UActGameplayAbility>(AbilityInstance))
			{
				OutAbilityInstance = ActAbility;
				return true;
			}
		}
	}

	return false;
}

bool UActAbilitySystemComponent::IsAbilityInstanceActive(const UActGameplayAbility* AbilityInstance)
{
	if (!AbilityInstance)
	{
		return false;
	}

	ABILITYLIST_SCOPE_LOCK();

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.IsActive())
		{
			continue;
		}

		const TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
		for (UGameplayAbility* Instance : Instances)
		{
			if (Instance == AbilityInstance)
			{
				return true;
			}
		}
	}

	return false;
}

void UActAbilitySystemComponent::CancelAbilityByHandle(const FGameplayAbilitySpecHandle AbilityHandle)
{
	if (AbilityHandle.IsValid())
	{
		CancelAbilityHandle(AbilityHandle);
	}
}

void UActAbilitySystemComponent::RebuildAbilityIdCache()
{
	AbilityIdToSpecHandle.Reset();

	ABILITYLIST_SCOPE_LOCK();

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		const UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec.Ability);
		if (!ActAbilityCDO)
		{
			continue;
		}

		const FName AbilityID = ActAbilityCDO->GetAbilityID();
		if (AbilityID.IsNone())
		{
			continue;
		}

		if (AbilityIdToSpecHandle.Contains(AbilityID))
		{
			UE_LOG(LogActAbilitySystem, Warning, TEXT("Duplicate AbilityID [%s] detected on ASC [%s]."), *AbilityID.ToString(), *GetNameSafe(this));
			continue;
		}

		AbilityIdToSpecHandle.Add(AbilityID, AbilitySpec.Handle);
	}
}

FGameplayAbilitySpecHandle UActAbilitySystemComponent::GetAbilitySpecHandleByID(const FName AbilityID) const
{
	if (AbilityID.IsNone())
	{
		return FGameplayAbilitySpecHandle();
	}

	if (const FGameplayAbilitySpecHandle* Handle = AbilityIdToSpecHandle.Find(AbilityID))
	{
		return *Handle;
	}

	return FGameplayAbilitySpecHandle();
}

bool UActAbilitySystemComponent::GetAbilityIdByInputTag(const FGameplayTag InputTag, FName& OutAbilityId) const
{
	OutAbilityId = NAME_None;

	if (!InputTag.IsValid())
	{
		return false;
	}
	
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		const UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec.Ability);
		if (!ActAbilityCDO)
		{
			continue;
		}

		const FName AbilityID = ActAbilityCDO->GetAbilityID();
		if (AbilityID.IsNone())
		{
			continue;
		}

		OutAbilityId = AbilityID;
		return true;
	}

	return false;
}

bool UActAbilitySystemComponent::TryActivateAbilityByID(
	const FName AbilityID,
	const bool bAllowRemoteActivation,
	const bool bCancelIfAlreadyActive,
	FGameplayAbilitySpecHandle* OutActivatedSpecHandle)
{
	if (AbilityID.IsNone())
	{
		return false;
	}

	const FGameplayAbilitySpecHandle Handle = GetAbilitySpecHandleByID(AbilityID);
	if (!Handle.IsValid())
	{
		return false;
	}

	if (!FindAbilitySpecFromHandle(Handle))
	{
		AbilityIdToSpecHandle.Remove(AbilityID);
		return false;
	}

	if (bCancelIfAlreadyActive)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle))
		{
			if (AbilitySpec->IsActive())
			{
				CancelAbilityHandle(Handle);
			}
		}
	}

	const bool bRequested = TryActivateAbility(Handle, bAllowRemoteActivation);
	if (!bRequested)
	{
		return false;
	}

	if (const FGameplayAbilitySpec* ActivatedSpec = FindAbilitySpecFromHandle(Handle))
	{
		if (ActivatedSpec->IsActive())
		{
			if (OutActivatedSpecHandle)
			{
				*OutActivatedSpecHandle = Handle;
			}
			return true;
		}
	}

	return false;
}

void UActAbilitySystemComponent::K2_AbilityInputTagPressed(const FGameplayTag InputTag)
{
	AbilityInputTagPressed(InputTag);
}

void UActAbilitySystemComponent::K2_AbilityInputTagReleased(const FGameplayTag InputTag)
{
	AbilityInputTagReleased(InputTag);
}

int32 UActAbilitySystemComponent::CreateBullet(FName BulletID, const FBulletInitParams& InitParams) const
{
	const AActPlayerState* ActPS = Cast<AActPlayerState>(GetOwnerActor());
	if (!ActPS) return  INDEX_NONE;

	const UActPawnData* PawnData = ActPS->GetPawnData<UActPawnData>();
	if (!PawnData) return  INDEX_NONE;
	
	return UBulletBlueprintLibrary::CreateBullet(ActPS, PawnData->BulletConfig, BulletID, InitParams);
}

int32 UActAbilitySystemComponent::K2_CreateBullet(FName BulletID, const FBulletInitParams& InitParams)
{
	return CreateBullet(BulletID, InitParams);
}

void UActAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (HasMatchingGameplayTag(TAG_Gameplay_AbilityInputBlocked))
	{
		ClearAbilityInput();
		return;
	}

	static TArray<FGameplayAbilitySpecHandle> AbilitiesToActive;
	AbilitiesToActive.Reset();

	//@TODO: See if we can use FScopedServerAbilityRPCBatcher ScopedRPCBatcher in some of these loops

	//
	// Process all abilities that activate when the input is held.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability && !AbilitySpec->IsActive())
			{
				const UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec->Ability);
				if (ActAbilityCDO && ActAbilityCDO->GetActivationPolicy() == EActAbilityActivationPolicy::WhileInputActive)
				{
					AbilitiesToActive.AddUnique(AbilitySpec->Handle);
				}
			}
		}
	}

	//
	// Process all abilities that had their input pressed this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = true;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputPressed(*AbilitySpec);
				}
				else
				{
					const UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec->Ability);

					if (ActAbilityCDO && ActAbilityCDO->GetActivationPolicy() == EActAbilityActivationPolicy::OnInputTriggered)
					{
						AbilitiesToActive.AddUnique(AbilitySpec->Handle);
					}
				}
			}
		}
	}

	//
	// Try to activate all the abilities that are from presses and holds.
	// We do it all at once so that held inputs don't activate the ability
	// and then also send an input event to the ability because of the press.
	//
	if (!AbilitiesToActive.IsEmpty())
	{
		for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActive)
		{
			TryActivateAbility(AbilitySpecHandle);
		}
	}

	//
	// Process all abilities that had their input released this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = false;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputReleased(*AbilitySpec);
				}
			}
		}
	}

	//
	// Clear the cached ability handles.
	//
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UActAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

bool UActAbilitySystemComponent::IsActivationGroupBlocked(EActAbilityActivationGroup Group) const
{
	bool bBlocked = false;

	switch (Group)
	{
	case EActAbilityActivationGroup::Independent:
		// Independent abilities are never blocked.
		bBlocked = false;
		break;

	case EActAbilityActivationGroup::Exclusive_Replaceable:
	case EActAbilityActivationGroup::Exclusive_Blocking:
		// Exclusive abilities can activate if nothing is blocking.
		bBlocked = (ActivationGroupCounts[static_cast<uint8>(EActAbilityActivationGroup::Exclusive_Blocking)] > 0);
		break;

	default:
		checkf(false, TEXT("IsActivationGroupBlocked: Invalid ActivationGroup [%d]\n"), (uint8)Group);
		break;
	}

	return bBlocked;
}

void UActAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags)
{
	FGameplayTagContainer ModifiedBlockTags = BlockTags;
	FGameplayTagContainer ModifiedCancelTags = CancelTags;

	if (TagRelationshipMapping)
	{
		// Use the mapping to expand the ability tags into block and cancel tag
		TagRelationshipMapping->GetAbilityTagsToBlockAndCancel(AbilityTags, &ModifiedBlockTags, &ModifiedCancelTags);
	}

	Super::ApplyAbilityBlockAndCancelTags(AbilityTags, RequestingAbility, bEnableBlockTags, ModifiedBlockTags, bExecuteCancelTags, ModifiedCancelTags);

	//@TODO: Apply any special logic like blocking input or movement
}

void UActAbilitySystemComponent::SetTagRelationshipMapping(UActAbilityTagRelationshipMapping* NewMapping)
{
	TagRelationshipMapping = NewMapping;
}

void UActAbilitySystemComponent::GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags,
	FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const
{
	if (TagRelationshipMapping)
	{
		TagRelationshipMapping->GetRequiredAndBlockedActivationTags(AbilityTags, &OutActivationRequired, &OutActivationBlocked);
	}
}

void UActAbilitySystemComponent::ResetGameplayTagCounts(FGameplayTagContainer TagContainer, const int32 NewCount)
{
	for (const FGameplayTag& Tag : TagContainer)
	{
		SetLooseGameplayTagCount(Tag, NewCount);
	}
}

void UActAbilitySystemComponent::ClientTryActivateAbility_Implementation(FGameplayAbilitySpecHandle AbilityToActivate)
{
	Super::ClientTryActivateAbility_Implementation(AbilityToActivate);
}

void UActAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);

	const UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec.Ability);
	if (!ActAbilityCDO)
	{
		return;
	}

	const FName AbilityID = ActAbilityCDO->GetAbilityID();
	if (AbilityID.IsNone())
	{
		return;
	}

	if (AbilityIdToSpecHandle.Contains(AbilityID))
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("Duplicate AbilityID [%s] detected on ASC [%s]."), *AbilityID.ToString(), *GetNameSafe(this));
		return;
	}

	AbilityIdToSpecHandle.Add(AbilityID, AbilitySpec.Handle);
}

void UActAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	if (const UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec.Ability))
	{
		const FName AbilityID = ActAbilityCDO->GetAbilityID();
		if (!AbilityID.IsNone())
		{
			const FGameplayAbilitySpecHandle* Handle = AbilityIdToSpecHandle.Find(AbilityID);
			if (Handle && *Handle == AbilitySpec.Handle)
			{
				AbilityIdToSpecHandle.Remove(AbilityID);
			}
		}
	}

	Super::OnRemoveAbility(AbilitySpec);
}

void UActAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();

	RebuildAbilityIdCache();
}

void UActAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	// We don't support UGameplayAbility::bReplicateInputDirectly.
	// Use replicated events instead so that the WaitInputPress ability task works.
	if (Spec.IsActive())
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		const FPredictionKey OriginalPredictionKey = Instance
			? Instance->GetCurrentActivationInfo().GetActivationPredictionKey()
			: Spec.ActivationInfo.GetActivationPredictionKey();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// Invoke the InputPressed event. This is not replicated here. If someone is listening, they may replicate the InputPressed event to the server.
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, OriginalPredictionKey);
	}
}

void UActAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	// We don't support UGameplayAbility::bReplicateInputDirectly.
	// Use replicated events instead so that the WaitInputRelease ability task works.
	if (Spec.IsActive())
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		const FPredictionKey OriginalPredictionKey = Instance
			? Instance->GetCurrentActivationInfo().GetActivationPredictionKey()
			: Spec.ActivationInfo.GetActivationPredictionKey();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// Invoke the InputReleased event. This is not replicated here. If someone is listening, they may replicate the InputReleased event to the server.
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, OriginalPredictionKey);
	}
}
