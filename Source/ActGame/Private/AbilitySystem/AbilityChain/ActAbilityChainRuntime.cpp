#include "AbilitySystem/AbilityChain/ActAbilityChainRuntime.h"

#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/ActGameplayAbility.h"

namespace
{
	/** Server-side validation grace after a combo window closes. */
	constexpr double AbilityChainValidationGraceSeconds = 0.35;

	/** Short-lived activation gate for predicted follow-ups under weak network conditions. */
	constexpr double AbilityChainAuthorizationLifetimeSeconds = 0.75;
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
	SourceActivationPredictionKey = 0;
	CommandTag = FGameplayTag();
	TargetAbilityId = NAME_None;
	SourceSpecHandle = FGameplayAbilitySpecHandle();
	AuthorizedAtSeconds = 0.0;
	ExpiresAtSeconds = 0.0;
}

bool FActAbilityChainRuntime::FPendingActivationAuthorization::IsExpired(const double CurrentTimeSeconds) const
{
	return CurrentTimeSeconds > ExpiresAtSeconds;
}

bool FActAbilityChainRuntime::FPendingActivationAuthorization::MatchesTarget(const FName AbilityId, const double CurrentTimeSeconds) const
{
	return !TargetAbilityId.IsNone() && TargetAbilityId == AbilityId && !IsExpired(CurrentTimeSeconds);
}

bool FActAbilityChainRuntime::FPendingActivationAuthorization::MatchesRequest(
	const FActAbilityChainActivationRequest& Request,
	const FGameplayAbilitySpecHandle InSourceSpecHandle,
	const double CurrentTimeSeconds) const
{
	if (IsExpired(CurrentTimeSeconds))
	{
		return false;
	}

	const bool bPredictionKeyMatches =
		SourceActivationPredictionKey == 0 ||
		Request.SourceActivationPredictionKey == 0 ||
		SourceActivationPredictionKey == Request.SourceActivationPredictionKey;

	return SourceAbilityId == Request.SourceAbilityId &&
		CommandTag == Request.CommandTag &&
		TargetAbilityId == Request.TargetAbilityId &&
		SourceSpecHandle == InSourceSpecHandle &&
		bPredictionKeyMatches;
}

void FActAbilityChainRuntime::Reset()
{
	ActiveContext.Reset();
	PendingActivations.Reset();
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
	if (HasPendingActivationForAbility(AbilityId, CurrentTimeSeconds))
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
	PruneExpiredPendingActivations(CurrentTimeSeconds);
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
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] Authorize accepted. Owner=%s SourceAbilityId=%s SourcePredKey=%d Command=%s TargetAbilityId=%s PendingCount=%d Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*Request.SourceAbilityId.ToString(),
		Request.SourceActivationPredictionKey,
		*Request.CommandTag.ToString(),
		*Request.TargetAbilityId.ToString(),
		PendingActivations.Num(),
		CurrentTimeSeconds);
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

int16 FActAbilityChainRuntime::GetActiveContextPredictionKey() const
{
	const UActGameplayAbility* ActiveAbility = ActiveContext.AbilityInstance.Get();
	if (!ActiveAbility)
	{
		return 0;
	}

	return ActiveAbility->GetCurrentActivationInfo().GetActivationPredictionKey().Current;
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

	const int16 ActiveSourcePredictionKey = GetActiveContextPredictionKey();
	if (Request.SourceActivationPredictionKey != 0 &&
		ActiveSourcePredictionKey != 0 &&
		Request.SourceActivationPredictionKey != ActiveSourcePredictionKey)
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

void FActAbilityChainRuntime::PruneExpiredPendingActivations(const double CurrentTimeSeconds)
{
	PendingActivations.RemoveAll([CurrentTimeSeconds](const FPendingActivationAuthorization& Authorization)
	{
		return Authorization.IsExpired(CurrentTimeSeconds);
	});
}

void FActAbilityChainRuntime::GrantPendingActivation(const FActAbilityChainActivationRequest& Request, const double CurrentTimeSeconds)
{
	const int32 ExistingAuthorizationIndex = FindPendingActivationIndexByRequest(Request, ActiveContext.SpecHandle, CurrentTimeSeconds);
	if (ExistingAuthorizationIndex != INDEX_NONE)
	{
		FPendingActivationAuthorization& ExistingAuthorization = PendingActivations[ExistingAuthorizationIndex];
		ExistingAuthorization.AuthorizedAtSeconds = CurrentTimeSeconds;
		ExistingAuthorization.ExpiresAtSeconds = CurrentTimeSeconds + AbilityChainAuthorizationLifetimeSeconds;
		return;
	}

	FPendingActivationAuthorization& NewAuthorization = PendingActivations.AddDefaulted_GetRef();
	NewAuthorization.SourceAbilityId = Request.SourceAbilityId;
	NewAuthorization.SourceActivationPredictionKey = Request.SourceActivationPredictionKey;
	NewAuthorization.CommandTag = Request.CommandTag;
	NewAuthorization.TargetAbilityId = Request.TargetAbilityId;
	NewAuthorization.SourceSpecHandle = ActiveContext.SpecHandle;
	NewAuthorization.AuthorizedAtSeconds = CurrentTimeSeconds;
	NewAuthorization.ExpiresAtSeconds = CurrentTimeSeconds + AbilityChainAuthorizationLifetimeSeconds;
}

bool FActAbilityChainRuntime::HasPendingActivationForAbility(const FName AbilityId, const double CurrentTimeSeconds) const
{
	return FindPendingActivationIndexByAbility(AbilityId, CurrentTimeSeconds) != INDEX_NONE;
}

int32 FActAbilityChainRuntime::FindPendingActivationIndexByAbility(const FName AbilityId, const double CurrentTimeSeconds) const
{
	for (int32 AuthorizationIndex = PendingActivations.Num() - 1; AuthorizationIndex >= 0; --AuthorizationIndex)
	{
		const FPendingActivationAuthorization& Authorization = PendingActivations[AuthorizationIndex];
		if (Authorization.MatchesTarget(AbilityId, CurrentTimeSeconds))
		{
			return AuthorizationIndex;
		}
	}

	return INDEX_NONE;
}

int32 FActAbilityChainRuntime::FindPendingActivationIndexByRequest(
	const FActAbilityChainActivationRequest& Request,
	const FGameplayAbilitySpecHandle SourceSpecHandle,
	const double CurrentTimeSeconds) const
{
	for (int32 AuthorizationIndex = PendingActivations.Num() - 1; AuthorizationIndex >= 0; --AuthorizationIndex)
	{
		const FPendingActivationAuthorization& Authorization = PendingActivations[AuthorizationIndex];
		if (Authorization.MatchesRequest(Request, SourceSpecHandle, CurrentTimeSeconds))
		{
			return AuthorizationIndex;
		}
	}

	return INDEX_NONE;
}

void FActAbilityChainRuntime::ClearPendingActivationIfMatches(const FName AbilityId)
{
	for (int32 AuthorizationIndex = PendingActivations.Num() - 1; AuthorizationIndex >= 0; --AuthorizationIndex)
	{
		if (PendingActivations[AuthorizationIndex].TargetAbilityId == AbilityId)
		{
			PendingActivations.RemoveAt(AuthorizationIndex);
			return;
		}
	}
}

void FActAbilityChainRuntime::CancelPreviousSourceAbilityIfNeeded(
	UActAbilitySystemComponent& ActASC,
	const FGameplayAbilitySpecHandle NewSpecHandle,
	const FName NewAbilityId)
{
	const double CurrentTimeSeconds = GetCurrentTimeSeconds(ActASC);
	PruneExpiredPendingActivations(CurrentTimeSeconds);

	const int32 AuthorizationIndex = FindPendingActivationIndexByAbility(NewAbilityId, CurrentTimeSeconds);
	if (AuthorizationIndex == INDEX_NONE)
	{
		return;
	}

	const FGameplayAbilitySpecHandle SourceSpecHandle = PendingActivations[AuthorizationIndex].SourceSpecHandle;
	PendingActivations.RemoveAt(AuthorizationIndex);

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
	Request.SourceActivationPredictionKey = GetActiveContextPredictionKey();
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
