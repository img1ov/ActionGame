#include "AbilitySystem/AbilityChain/ActAbilityChainRuntime.h"

#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/ActGameplayAbility.h"

namespace
{
	/** Server-side validation grace after a combo window closes. */
	constexpr double AbilityChainValidationGraceSeconds = 0.20;

	/** Short-lived activation gate for one predicted follow-up ability. */
	constexpr double AbilityChainAuthorizationLifetimeSeconds = 0.35;
}

void FActAbilityChainRuntime::FActiveChainContext::Reset()
{
	AbilityInstance.Reset();
	SpecHandle = FGameplayAbilitySpecHandle();
	AbilityId = NAME_None;
}

bool FActAbilityChainRuntime::FActiveChainContext::IsValid() const
{
	return AbilityInstance.IsValid() && SpecHandle.IsValid() && !AbilityId.IsNone();
}

void FActAbilityChainRuntime::FPendingActivationAuthorization::Reset()
{
	SourceAbilityId = NAME_None;
	TargetAbilityId = NAME_None;
	SourceSpecHandle = FGameplayAbilitySpecHandle();
	ExpiresAtSeconds = 0.0;
}

bool FActAbilityChainRuntime::FPendingActivationAuthorization::Matches(const FName AbilityId, const double CurrentTimeSeconds) const
{
	return !TargetAbilityId.IsNone() && TargetAbilityId == AbilityId && CurrentTimeSeconds <= ExpiresAtSeconds;
}

void FActAbilityChainRuntime::Reset()
{
	ActiveContext.Reset();
	PendingActivation.Reset();
	ResetWindowStates();
	NextWindowOpenSequence = 0;
}

bool FActAbilityChainRuntime::HasActiveChain(const UActAbilitySystemComponent& ActASC) const
{
	return HasUsableActiveContext(ActASC);
}

void FActAbilityChainRuntime::BeginAbility(UActAbilitySystemComponent& ActASC, UActGameplayAbility& Ability, const FGameplayAbilitySpecHandle SpecHandle)
{
	const FName AbilityId = Ability.GetAbilityId();
	if (AbilityId.IsNone() || !SpecHandle.IsValid())
	{
		return;
	}

	// Once the follow-up ability has really started, the previous attack can be retired safely.
	CancelPreviousSourceAbilityIfNeeded(ActASC, SpecHandle, AbilityId);

	ActiveContext.Reset();
	ActiveContext.AbilityInstance = &Ability;
	ActiveContext.SpecHandle = SpecHandle;
	ActiveContext.AbilityId = AbilityId;
	ResetWindowStates();

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] Begin. Owner=%s AbilityId=%s Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*AbilityId.ToString(),
		GetCurrentTimeSeconds(ActASC));
}

void FActAbilityChainRuntime::EndAbility(UActAbilitySystemComponent& ActASC, const UActGameplayAbility& Ability, const FGameplayAbilitySpecHandle SpecHandle)
{
	if (!ActiveContext.IsValid())
	{
		return;
	}

	const bool bMatchesAbility = ActiveContext.AbilityInstance.Get() == &Ability || ActiveContext.AbilityId == Ability.GetAbilityId();
	const bool bMatchesSpecHandle = !SpecHandle.IsValid() || ActiveContext.SpecHandle == SpecHandle;
	if (!bMatchesAbility || !bMatchesSpecHandle)
	{
		return;
	}

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] End. Owner=%s AbilityId=%s Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityId.ToString(),
		GetCurrentTimeSeconds(ActASC));

	ActiveContext.Reset();
	ResetWindowStates();
}

void FActAbilityChainRuntime::OpenWindow(UActAbilitySystemComponent& ActASC, const FActAbilityChainWindowDefinition& WindowDefinition)
{
	if (ResetIfActiveContextInvalid(ActASC) || !ActiveContext.IsValid() || !WindowDefinition.IsValid())
	{
		return;
	}

	FWindowState& WindowState = WindowStates.FindOrAdd(WindowDefinition.WindowId);
	WindowState.Definition = WindowDefinition;
	WindowState.OpenCount = FMath::Max(0, WindowState.OpenCount) + 1;
	WindowState.OpenSequence = ++NextWindowOpenSequence;

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] WindowOpen. Owner=%s AbilityId=%s WindowId=%s Priority=%d Count=%d Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityId.ToString(),
		*WindowDefinition.WindowId.ToString(),
		WindowDefinition.WindowPriority,
		WindowState.OpenCount,
		GetCurrentTimeSeconds(ActASC));
}

void FActAbilityChainRuntime::CloseWindow(UActAbilitySystemComponent& ActASC, const FName WindowId)
{
	if (ResetIfActiveContextInvalid(ActASC) || !ActiveContext.IsValid() || WindowId.IsNone())
	{
		return;
	}

	FWindowState* WindowState = WindowStates.Find(WindowId);
	if (!WindowState)
	{
		return;
	}

	WindowState->OpenCount = FMath::Max(0, WindowState->OpenCount - 1);
	if (!WindowState->IsOpen())
	{
		// Closed windows stay warm for a short time so the server can still validate
		// late-arriving predicted follow-up requests under weak network conditions.
		WindowState->LastClosedTimeSeconds = GetCurrentTimeSeconds(ActASC);
	}

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] WindowClose. Owner=%s AbilityId=%s WindowId=%s Count=%d Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityId.ToString(),
		*WindowId.ToString(),
		WindowState->OpenCount,
		GetCurrentTimeSeconds(ActASC));
}

bool FActAbilityChainRuntime::CanActivateAbility(const UActAbilitySystemComponent& ActASC, const UActGameplayAbility& Ability, FGameplayTagContainer* OptionalRelevantTags) const
{
	const FName AbilityId = Ability.GetAbilityId();
	if (AbilityId.IsNone())
	{
		return true;
	}

	const double CurrentTimeSeconds = GetCurrentTimeSeconds(ActASC);
	if (PendingActivation.Matches(AbilityId, CurrentTimeSeconds))
	{
		return true;
	}

	if (!HasUsableActiveContext(ActASC))
	{
		return true;
	}

	if (OptionalRelevantTags)
	{
		OptionalRelevantTags->AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.ActivateFail.Chain"), false));
	}

	return false;
}

FActAbilityChainCommandResolveResult FActAbilityChainRuntime::ResolveCommand(UActAbilitySystemComponent& ActASC, const FGameplayTag& CommandTag)
{
	FActAbilityChainCommandResolveResult Result;
	if (ResetIfActiveContextInvalid(ActASC) || !ActiveContext.IsValid() || !CommandTag.IsValid())
	{
		return Result;
	}

	TArray<const FWindowState*> CandidateWindows;
	GatherCandidateWindows(CommandTag, GetCurrentTimeSeconds(ActASC), false, CandidateWindows);

	for (const FWindowState* WindowState : CandidateWindows)
	{
		if (!WindowState)
		{
			continue;
		}

		const FActAbilityChainEntry* Entry = FindBestMatchingEntryInWindow(*WindowState, CommandTag);
		if (!Entry)
		{
			continue;
		}

		if (!TryActivateChainEntry(ActASC, CommandTag, *Entry))
		{
			continue;
		}

		Result.Resolution = EActAbilityChainCommandResolution::AbilityActivated;
		Result.SourceAbilityId = ActiveContext.AbilityId;
		Result.TargetAbilityId = Entry->TargetAbilityId;
		Result.bConsumeInput = true;
		return Result;
	}

	return Result;
}

bool FActAbilityChainRuntime::AuthorizePredictedActivation(UActAbilitySystemComponent& ActASC, const FActAbilityChainActivationRequest& Request)
{
	if (!Request.IsValid() || ResetIfActiveContextInvalid(ActASC) || !ActiveContext.IsValid())
	{
		return false;
	}

	const double CurrentTimeSeconds = GetCurrentTimeSeconds(ActASC);
	if (!IsAuthorizedRequest(Request, CurrentTimeSeconds))
	{
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] Authorize rejected. Owner=%s SourceAbilityId=%s Command=%s TargetAbilityId=%s Time=%.3f"),
			*GetNameSafe(ActASC.GetAvatarActor()),
			*Request.SourceAbilityId.ToString(),
			*Request.CommandTag.ToString(),
			*Request.TargetAbilityId.ToString(),
			CurrentTimeSeconds);
		return false;
	}

	GrantPendingActivation(Request, CurrentTimeSeconds);
	return true;
}

void FActAbilityChainRuntime::NotifyAbilityActivationFailed(const FName AbilityId)
{
	ClearPendingActivationIfMatches(AbilityId);
}

double FActAbilityChainRuntime::GetCurrentTimeSeconds(const UActAbilitySystemComponent& ActASC) const
{
	const UWorld* World = ActASC.GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

bool FActAbilityChainRuntime::HasUsableActiveContext(const UActAbilitySystemComponent& ActASC) const
{
	if (!ActiveContext.IsValid())
	{
		return false;
	}

	const FGameplayAbilitySpec* ActiveSpec = ActASC.FindAbilitySpecFromHandle(ActiveContext.SpecHandle);
	return ActiveSpec && ActiveSpec->IsActive();
}

bool FActAbilityChainRuntime::ResetIfActiveContextInvalid(UActAbilitySystemComponent& ActASC)
{
	if (!ActiveContext.IsValid() || HasUsableActiveContext(ActASC))
	{
		return false;
	}

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] Reset stale active context. Owner=%s AbilityId=%s Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityId.ToString(),
		GetCurrentTimeSeconds(ActASC));

	Reset();
	return true;
}

void FActAbilityChainRuntime::ResetWindowStates()
{
	WindowStates.Reset();
}

bool FActAbilityChainRuntime::IsWindowAvailableForValidation(const FWindowState& WindowState, const double CurrentTimeSeconds) const
{
	return WindowState.IsOpen() || (CurrentTimeSeconds - WindowState.LastClosedTimeSeconds) <= AbilityChainValidationGraceSeconds;
}

void FActAbilityChainRuntime::GatherCandidateWindows(
	const FGameplayTag& CommandTag,
	const double CurrentTimeSeconds,
	const bool bAllowValidationGrace,
	TArray<const FWindowState*>& OutWindows) const
{
	OutWindows.Reset();

	for (const TPair<FName, FWindowState>& Pair : WindowStates)
	{
		const FWindowState& WindowState = Pair.Value;
		const bool bWindowAvailable = bAllowValidationGrace
			? IsWindowAvailableForValidation(WindowState, CurrentTimeSeconds)
			: WindowState.IsOpen();
		if (!bWindowAvailable)
		{
			continue;
		}

		if (!FindBestMatchingEntryInWindow(WindowState, CommandTag))
		{
			continue;
		}

		OutWindows.Add(&WindowState);
	}

	OutWindows.Sort([](const FWindowState& Left, const FWindowState& Right)
	{
		if (Left.Definition.WindowPriority != Right.Definition.WindowPriority)
		{
			return Left.Definition.WindowPriority > Right.Definition.WindowPriority;
		}

		return Left.OpenSequence > Right.OpenSequence;
	});
}

const FActAbilityChainEntry* FActAbilityChainRuntime::FindBestMatchingEntryInWindow(const FWindowState& WindowState, const FGameplayTag& CommandTag) const
{
	const FActAbilityChainEntry* BestEntry = nullptr;

	for (const FActAbilityChainEntry& Entry : WindowState.Definition.Entries)
	{
		if (!Entry.IsValid() || Entry.CommandTag != CommandTag)
		{
			continue;
		}

		if (!BestEntry || Entry.Priority > BestEntry->Priority)
		{
			BestEntry = &Entry;
		}
	}

	return BestEntry;
}

bool FActAbilityChainRuntime::IsAuthorizedRequest(const FActAbilityChainActivationRequest& Request, const double CurrentTimeSeconds) const
{
	if (!ActiveContext.IsValid() || Request.SourceAbilityId != ActiveContext.AbilityId)
	{
		return false;
	}

	TArray<const FWindowState*> CandidateWindows;
	GatherCandidateWindows(Request.CommandTag, CurrentTimeSeconds, true, CandidateWindows);

	for (const FWindowState* WindowState : CandidateWindows)
	{
		if (!WindowState)
		{
			continue;
		}

		for (const FActAbilityChainEntry& Entry : WindowState->Definition.Entries)
		{
			if (!Entry.IsValid())
			{
				continue;
			}

			if (Entry.CommandTag == Request.CommandTag && Entry.TargetAbilityId == Request.TargetAbilityId)
			{
				return true;
			}
		}
	}

	return false;
}

void FActAbilityChainRuntime::GrantPendingActivation(const FActAbilityChainActivationRequest& Request, const double CurrentTimeSeconds)
{
	PendingActivation.SourceAbilityId = Request.SourceAbilityId;
	PendingActivation.TargetAbilityId = Request.TargetAbilityId;
	PendingActivation.SourceSpecHandle = ActiveContext.SpecHandle;
	PendingActivation.ExpiresAtSeconds = CurrentTimeSeconds + AbilityChainAuthorizationLifetimeSeconds;
}

void FActAbilityChainRuntime::ClearPendingActivationIfMatches(const FName AbilityId)
{
	if (PendingActivation.TargetAbilityId == AbilityId)
	{
		PendingActivation.Reset();
	}
}

void FActAbilityChainRuntime::CancelPreviousSourceAbilityIfNeeded(
	UActAbilitySystemComponent& ActASC,
	const FGameplayAbilitySpecHandle NewSpecHandle,
	const FName NewAbilityId)
{
	const double CurrentTimeSeconds = GetCurrentTimeSeconds(ActASC);
	if (!PendingActivation.Matches(NewAbilityId, CurrentTimeSeconds))
	{
		return;
	}

	const FGameplayAbilitySpecHandle SourceSpecHandle = PendingActivation.SourceSpecHandle;
	PendingActivation.Reset();

	if (!SourceSpecHandle.IsValid() || SourceSpecHandle == NewSpecHandle)
	{
		return;
	}

	// A successful follow-up replaces the previous combo source ability.
	ActASC.CancelAbilityByHandle(SourceSpecHandle);
}

bool FActAbilityChainRuntime::TryActivateChainEntry(
	UActAbilitySystemComponent& ActASC,
	const FGameplayTag& CommandTag,
	const FActAbilityChainEntry& Entry)
{
	if (!ActiveContext.IsValid() || !Entry.IsValid())
	{
		return false;
	}

	FActAbilityChainActivationRequest Request;
	Request.SourceAbilityId = ActiveContext.AbilityId;
	Request.CommandTag = CommandTag;
	Request.TargetAbilityId = Entry.TargetAbilityId;

	// Local prediction must pass through the same authorization function used by the server RPC path.
	if (!ActASC.AuthorizePredictedAbilityChainActivation(Request))
	{
		return false;
	}

	if (ActASC.TryActivateAbilityById(Entry.TargetAbilityId, true, false, nullptr))
	{
		return true;
	}

	ClearPendingActivationIfMatches(Entry.TargetAbilityId);
	return false;
}
