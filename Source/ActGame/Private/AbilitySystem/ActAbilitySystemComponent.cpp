// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/ActAbilitySystemComponent.h"

#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilityTagRelationshipMapping.h"
#include "AbilitySystem/Abilities/ActGameplayAbility.h"
#include "AbilitySystem/AbilityChain/ActAbilityChainRuntime.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityRepAnimMontage.h"
#include "Animation/ActAnimInstance.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Gameplay_AbilityInputBlocked, "Gameplay.AbilityInputBlocked");

namespace
{
double GetASCLogTimeSeconds(const UActAbilitySystemComponent* ASC)
{
	const UWorld* World = ASC ? ASC->GetWorld() : nullptr;
	return World ? World->GetTimeSeconds() : 0.0;
}

FString GetASCActorName(const UActAbilitySystemComponent* ASC)
{
	const FGameplayAbilityActorInfo* ActorInfo = ASC ? ASC->AbilityActorInfo.Get() : nullptr;
	return GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
}

FString GetAbilityDebugName(const UGameplayAbility* Ability)
{
	if (const UActGameplayAbility* ActAbility = Cast<UActGameplayAbility>(Ability))
	{
		return FString::Printf(TEXT("%s(%s)"), *GetNameSafe(ActAbility), *ActAbility->GetAbilityId().ToString());
	}

	return GetNameSafe(Ability);
}
}

UActAbilitySystemComponent::UActAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityChainRuntime = MakeUnique<FActAbilityChainRuntime>();
	SetMontageRepAnimPositionMethod(ERepAnimPositionMethod::CurrentSectionId);
}

UActAbilitySystemComponent::~UActAbilitySystemComponent() = default;

void UActAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	check(ActorInfo);
	check(InOwnerActor);
	
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	ResetAbilityChainRuntime();
	SetMontageRepAnimPositionMethod(ERepAnimPositionMethod::CurrentSectionId);

	if (UActAnimInstance* ActAnimInst = Cast<UActAnimInstance>(ActorInfo->GetAnimInstance()))
	{
		ActAnimInst->InitializeWithAbilitySystem(this);
	}
}

void UActAbilitySystemComponent::ResetAbilityChainRuntime()
{
	if (AbilityChainRuntime.IsValid())
	{
		AbilityChainRuntime->Reset();
	}
}

bool UActAbilitySystemComponent::HasActiveAbilityChain() const
{
	return AbilityChainRuntime.IsValid() && AbilityChainRuntime->HasActiveChain(*this);
}

bool UActAbilitySystemComponent::CanActivateAbilityForChain(const UActGameplayAbility& Ability, FGameplayTagContainer* OptionalRelevantTags) const
{
	return !AbilityChainRuntime.IsValid() || AbilityChainRuntime->CanActivateAbility(*this, Ability, OptionalRelevantTags);
}

void UActAbilitySystemComponent::BeginAbilityChain(UActGameplayAbility& Ability, const FGameplayAbilitySpecHandle SpecHandle)
{
	if (AbilityChainRuntime.IsValid())
	{
		AbilityChainRuntime->BeginAbility(*this, Ability, SpecHandle);
	}
}

void UActAbilitySystemComponent::EndAbilityChain(const UActGameplayAbility& Ability, const FGameplayAbilitySpecHandle SpecHandle)
{
	if (AbilityChainRuntime.IsValid())
	{
		AbilityChainRuntime->EndAbility(*this, Ability, SpecHandle);
	}
}

void UActAbilitySystemComponent::OpenAbilityChainWindow(const FActAbilityChainWindowDefinition& WindowDefinition)
{
	if (AbilityChainRuntime.IsValid())
	{
		AbilityChainRuntime->OpenWindow(*this, WindowDefinition);
	}
}

void UActAbilitySystemComponent::CloseAbilityChainWindow(const FName WindowId)
{
	if (AbilityChainRuntime.IsValid())
	{
		AbilityChainRuntime->CloseWindow(*this, WindowId);
	}
}

FActAbilityChainCommandResolveResult UActAbilitySystemComponent::ResolveAbilityChainCommand(const FGameplayTag& CommandTag)
{
	if (!AbilityChainRuntime.IsValid())
	{
		return FActAbilityChainCommandResolveResult();
	}

	return AbilityChainRuntime->ResolveCommand(*this, CommandTag);
}

bool UActAbilitySystemComponent::AuthorizePredictedAbilityChainActivation(const FActAbilityChainActivationRequest& Request)
{
	if (!Request.IsValid() || !AbilityChainRuntime.IsValid())
	{
		return false;
	}

	const bool bAuthorizedLocally = AbilityChainRuntime->AuthorizePredictedActivation(*this, Request);
	if (!bAuthorizedLocally)
	{
		return false;
	}

	if (ShouldForwardAbilityChainAuthorizationToServer())
	{
		ServerAuthorizeAbilityChainActivation(Request);
	}

	return true;
}

void UActAbilitySystemComponent::CancelAbilitiesByFunc(const TShouldCancelAbilityFunc& ShouldCancelFunc, const bool bReplicateCancelAbility)
{
	(void)bReplicateCancelAbility;

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

		// NonInstanced is deprecated in newer UE versions. Enforce an instanced policy without referencing the deprecated enumerator.
		const EGameplayAbilityInstancingPolicy::Type Policy = AbilitySpec.Ability->GetInstancingPolicy();
		ensureMsgf(
			Policy == EGameplayAbilityInstancingPolicy::InstancedPerActor || Policy == EGameplayAbilityInstancingPolicy::InstancedPerExecution,
			TEXT("CancelAbilitiesByFunc: All Abilities should be instanced (InstancedPerActor/InstancedPerExecution)."));

		// Cancel all the spawned instances.
		TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
		for (UGameplayAbility* AbilityInstance : Instances)
		{
			UActGameplayAbility* ActAbilityInstance = CastChecked<UActGameplayAbility>(AbilityInstance);

			if (ShouldCancelFunc(ActAbilityInstance, AbilitySpec.Handle))
			{
				if (ActAbilityInstance->CanBeCanceled())
				{
					CancelAbilityHandle(AbilitySpec.Handle);
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
	auto ShouldCancelFunc = [this](const UActGameplayAbility* ActAbility, FGameplayAbilitySpecHandle /*Handle*/)
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
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleInput] Pressed. Owner=%s Tag=%s Local=%d Auth=%d Time=%.3f"),
			*GetASCActorName(this),
			*InputTag.ToString(),
			AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
			AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
			GetASCLogTimeSeconds(this));

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
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleInput] Triggered. Owner=%s Tag=%s Local=%d Auth=%d Time=%.3f"),
			*GetASCActorName(this),
			*InputTag.ToString(),
			AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
			AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
			GetASCLogTimeSeconds(this));

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
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleInput] Released. Owner=%s Tag=%s Local=%d Auth=%d Time=%.3f"),
			*GetASCActorName(this),
			*InputTag.ToString(),
			AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
			AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
			GetASCLogTimeSeconds(this));

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

		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] TryActivateByClass request. Owner=%s Ability=%s Class=%s Handle=%s Local=%d Auth=%d Time=%.3f"),
			*GetASCActorName(this),
			*GetAbilityDebugName(AbilitySpec.Ability),
			*GetNameSafe(AbilityClass.Get()),
			*AbilitySpec.Handle.ToString(),
			AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
			AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
			GetASCLogTimeSeconds(this));

		const bool bRequested = TryActivateAbility(AbilitySpec.Handle, bAllowRemoteActivation);
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] TryActivateByClass result. Owner=%s Ability=%s Class=%s Handle=%s Requested=%d ActiveNow=%d Time=%.3f"),
			*GetASCActorName(this),
			*GetAbilityDebugName(AbilitySpec.Ability),
			*GetNameSafe(AbilityClass.Get()),
			*AbilitySpec.Handle.ToString(),
			bRequested,
			(FindAbilitySpecFromHandle(AbilitySpec.Handle) && FindAbilitySpecFromHandle(AbilitySpec.Handle)->IsActive()) ? 1 : 0,
			GetASCLogTimeSeconds(this));
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

		const FName AbilityId = ActAbilityCDO->GetAbilityId();
		if (AbilityId.IsNone())
		{
			continue;
		}

		if (AbilityIdToSpecHandle.Contains(AbilityId))
		{
			UE_LOG(LogActAbilitySystem, Warning, TEXT("Duplicate AbilityId [%s] detected on ASC [%s]."), *AbilityId.ToString(), *GetNameSafe(this));
			continue;
		}

		AbilityIdToSpecHandle.Add(AbilityId, AbilitySpec.Handle);
	}
}

FGameplayAbilitySpecHandle UActAbilitySystemComponent::GetAbilitySpecHandleById(const FName AbilityId) const
{
	if (AbilityId.IsNone())
	{
		return FGameplayAbilitySpecHandle();
	}

	if (const FGameplayAbilitySpecHandle* Handle = AbilityIdToSpecHandle.Find(AbilityId))
	{
		return *Handle;
	}

	return FGameplayAbilitySpecHandle();
}

bool UActAbilitySystemComponent::TryActivateAbilityById(
	const FName AbilityId,
	const bool bAllowRemoteActivation,
	const bool bCancelIfAlreadyActive,
	FGameplayAbilitySpecHandle* OutActivatedSpecHandle)
{
	if (AbilityId.IsNone())
	{
		return false;
	}

	const FGameplayAbilitySpecHandle Handle = GetAbilitySpecHandleById(AbilityId);
	if (!Handle.IsValid())
	{
		return false;
	}

	if (!FindAbilitySpecFromHandle(Handle))
	{
		AbilityIdToSpecHandle.Remove(AbilityId);
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

	const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle);
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] TryActivateById request. Owner=%s AbilityId=%s Ability=%s Handle=%s Local=%d Auth=%d Time=%.3f"),
		*GetASCActorName(this),
		*AbilityId.ToString(),
		AbilitySpec ? *GetAbilityDebugName(AbilitySpec->Ability) : TEXT("None"),
		*Handle.ToString(),
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
		GetASCLogTimeSeconds(this));

	const bool bRequested = TryActivateAbility(Handle, bAllowRemoteActivation);
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] TryActivateById result. Owner=%s AbilityId=%s Handle=%s Requested=%d ActiveNow=%d Time=%.3f"),
		*GetASCActorName(this),
		*AbilityId.ToString(),
		*Handle.ToString(),
		bRequested,
		(FindAbilitySpecFromHandle(Handle) && FindAbilitySpecFromHandle(Handle)->IsActive()) ? 1 : 0,
		GetASCLogTimeSeconds(this));
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


void UActAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (const UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec.Ability))
		{
			ActAbilityCDO->TryActivateAbilityOnSpawn(AbilityActorInfo.Get(), AbilitySpec);
		}
	}
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
				if (ActAbilityCDO &&
					ActAbilityCDO->GetActivationPolicy() == EActAbilityActivationPolicy::WhileInputActive)
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
					if (ActAbilityCDO &&
						ActAbilityCDO->GetActivationPolicy() == EActAbilityActivationPolicy::OnInputTriggered)
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
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] ClientTryActivateAbility RPC. Owner=%s Handle=%s Local=%d Auth=%d Time=%.3f"),
		*GetASCActorName(this),
		*AbilityToActivate.ToString(),
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
		GetASCLogTimeSeconds(this));

	Super::ClientTryActivateAbility_Implementation(AbilityToActivate);
}

void UActAbilitySystemComponent::ClientActivateAbilityFailed_Implementation(FGameplayAbilitySpecHandle AbilityToActivate, int16 PredictionKey)
{
	const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilityToActivate);
	UE_LOG(LogActAbilitySystem, Warning, TEXT("[BattleAbility] ClientActivateAbilityFailed RPC. Owner=%s Handle=%s Ability=%s PredictionKey=%d Local=%d Auth=%d Time=%.3f"),
		*GetASCActorName(this),
		*AbilityToActivate.ToString(),
		AbilitySpec ? *GetAbilityDebugName(AbilitySpec->Ability) : TEXT("None"),
		PredictionKey,
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
		GetASCLogTimeSeconds(this));

	if (ShouldPreserveClientAbilityPresentationOnActivationFailure(AbilitySpec))
	{
		PreserveClientAbilityPresentationOnActivationFailure(AbilityToActivate, PredictionKey);
		return;
	}

	Super::ClientActivateAbilityFailed_Implementation(AbilityToActivate, PredictionKey);
}

void UActAbilitySystemComponent::ServerAuthorizeAbilityChainActivation_Implementation(const FActAbilityChainActivationRequest& Request)
{
	AuthorizePredictedAbilityChainActivation(Request);
}

bool UActAbilitySystemComponent::ShouldForwardAbilityChainAuthorizationToServer() const
{
	return AbilityActorInfo.IsValid() && AbilityActorInfo->IsLocallyControlled() && !AbilityActorInfo->IsNetAuthority();
}

bool UActAbilitySystemComponent::ShouldPreserveClientAbilityPresentationOnActivationFailure(const FGameplayAbilitySpec* AbilitySpec) const
{
	if (!AbilityActorInfo.IsValid() || !AbilityActorInfo->IsLocallyControlled() || AbilityActorInfo->IsNetAuthority())
	{
		return false;
	}

	const UActGameplayAbility* ActAbility = AbilitySpec ? Cast<UActGameplayAbility>(AbilitySpec->Ability) : nullptr;
	if (!ActAbility)
	{
		return false;
	}

	// Combo abilities carry a stable AbilityId. For these abilities, client presentation smoothness
	// takes priority over server confirmation because gameplay authority remains on the server.
	return !ActAbility->GetAbilityId().IsNone();
}

void UActAbilitySystemComponent::PreserveClientAbilityPresentationOnActivationFailure(
	const FGameplayAbilitySpecHandle AbilityHandle,
	const int16 PredictionKey)
{
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilityHandle);
	if (!AbilitySpec)
	{
		return;
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (AbilitySpec->ActivationInfo.GetActivationPredictionKey().Current == PredictionKey)
	{
		AbilitySpec->ActivationInfo.SetActivationConfirmed();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogActAbilitySystem, Warning, TEXT("[BattleAbility] Preserve client presentation after server reject. Owner=%s Handle=%s Ability=%s PredictionKey=%d Time=%.3f"),
		*GetASCActorName(this),
		*AbilityHandle.ToString(),
		AbilitySpec->Ability ? *GetAbilityDebugName(AbilitySpec->Ability) : TEXT("None"),
		PredictionKey,
		GetASCLogTimeSeconds(this));
}

void UActAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);

	const UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec.Ability);
	if (!ActAbilityCDO)
	{
		return;
	}

	const FName AbilityId = ActAbilityCDO->GetAbilityId();
	if (AbilityId.IsNone())
	{
		return;
	}

	if (AbilityIdToSpecHandle.Contains(AbilityId))
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("Duplicate AbilityId [%s] detected on ASC [%s]."), *AbilityId.ToString(), *GetNameSafe(this));
		return;
	}

	AbilityIdToSpecHandle.Add(AbilityId, AbilitySpec.Handle);
}

void UActAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	if (const UActGameplayAbility* ActAbilityCDO = Cast<UActGameplayAbility>(AbilitySpec.Ability))
	{
		const FName AbilityId = ActAbilityCDO->GetAbilityId();
		if (!AbilityId.IsNone())
		{
			const FGameplayAbilitySpecHandle* Handle = AbilityIdToSpecHandle.Find(AbilityId);
			if (Handle && *Handle == AbilitySpec.Handle)
			{
				AbilityIdToSpecHandle.Remove(AbilityId);
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

void UActAbilitySystemComponent::NotifyAbilityCommit(UGameplayAbility* Ability)
{
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] Commit. Owner=%s Ability=%s Local=%d Auth=%d Time=%.3f"),
		*GetASCActorName(this),
		*GetAbilityDebugName(Ability),
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
		GetASCLogTimeSeconds(this));

	Super::NotifyAbilityCommit(Ability);
}

void UActAbilitySystemComponent::NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] Activated notify. Owner=%s Handle=%s Ability=%s Local=%d Auth=%d Time=%.3f"),
		*GetASCActorName(this),
		*Handle.ToString(),
		*GetAbilityDebugName(Ability),
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
		GetASCLogTimeSeconds(this));

	Super::NotifyAbilityActivated(Handle, Ability);
}

void UActAbilitySystemComponent::NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason)
{
	NotifyAbilityChainActivationFailed(Ability);

	UE_LOG(LogActAbilitySystem, Warning, TEXT("[BattleAbility] Activate failed notify. Owner=%s Handle=%s Ability=%s Reasons=%s Local=%d Auth=%d Time=%.3f"),
		*GetASCActorName(this),
		*Handle.ToString(),
		*GetAbilityDebugName(Ability),
		*FailureReason.ToString(),
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsLocallyControlled() : 0,
		AbilityActorInfo.IsValid() ? AbilityActorInfo->IsNetAuthority() : 0,
		GetASCLogTimeSeconds(this));

	Super::NotifyAbilityFailed(Handle, Ability, FailureReason);
}

void UActAbilitySystemComponent::NotifyAbilityChainActivationFailed(UGameplayAbility* Ability)
{
	if (!AbilityChainRuntime.IsValid())
	{
		return;
	}

	const UActGameplayAbility* ActAbility = Cast<UActGameplayAbility>(Ability);
	if (!ActAbility)
	{
		return;
	}

	AbilityChainRuntime->NotifyAbilityActivationFailed(ActAbility->GetAbilityId());
}


void UActAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	// We don't support UGameplayAbility::bReplicateInputDirectly.
	// Use replicated events instead so that the WaitInputPress ability task works.
	if (Spec.IsActive())
	{
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		if (!Instance)
		{
			// InstancedPerExecution may not have a "primary" instance. Fall back to any active instance.
			const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
			Instance = Instances.Num() > 0 ? Instances[0] : nullptr;
		}

		const FPredictionKey OriginalPredictionKey = Instance
			? Instance->GetCurrentActivationInfo().GetActivationPredictionKey()
			: FPredictionKey();

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
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		if (!Instance)
		{
			const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
			Instance = Instances.Num() > 0 ? Instances[0] : nullptr;
		}

		const FPredictionKey OriginalPredictionKey = Instance
			? Instance->GetCurrentActivationInfo().GetActivationPredictionKey()
			: FPredictionKey();

		// Invoke the InputReleased event. This is not replicated here. If someone is listening, they may replicate the InputReleased event to the server.
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, OriginalPredictionKey);
	}
}
