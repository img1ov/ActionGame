#include "AbilitySystem/AbilityChain/ActAbilityChainRuntime.h"

#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/ActGameplayAbility.h"
#include "AbilitySystem/AbilityChain/ActAbilityChainData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

namespace
{
double GetChainLogTimeSeconds(const UActAbilitySystemComponent& ActASC)
{
	const UWorld* World = ActASC.GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}
}

void FActAbilityChainRuntime::FActiveChainContext::Reset()
{
	AbilityInstance.Reset();
	ChainData.Reset();
	SpecHandle = FGameplayAbilitySpecHandle();
	AbilityID = NAME_None;
	CurrentNodeId = NAME_None;
	WindowStates.Reset();
	LastNodeChangeTime = 0.0;
}

bool FActAbilityChainRuntime::FActiveChainContext::IsValid() const
{
	return AbilityInstance.IsValid() && ChainData.IsValid() && !AbilityID.IsNone();
}

void FActAbilityChainRuntime::FRecentChainContext::Reset()
{
	ChainData.Reset();
	AbilityID = NAME_None;
	CurrentNodeId = NAME_None;
	LastClosedWindowStates.Reset();
	ExpiresAt = 0.0;
}

bool FActAbilityChainRuntime::FRecentChainContext::IsValid(const double CurrentTimeSeconds) const
{
	return ChainData.IsValid() && !AbilityID.IsNone() && CurrentTimeSeconds <= ExpiresAt;
}

void FActAbilityChainRuntime::FPendingAbilityAuthorization::Reset()
{
	SourceAbilityID = NAME_None;
	SourceNodeId = NAME_None;
	TargetAbilityID = NAME_None;
	ExpiresAt = 0.0;
}

bool FActAbilityChainRuntime::FPendingAbilityAuthorization::Matches(const FName InAbilityID, const double CurrentTimeSeconds) const
{
	return !TargetAbilityID.IsNone() && TargetAbilityID == InAbilityID && CurrentTimeSeconds <= ExpiresAt;
}

void FActAbilityChainRuntime::Reset()
{
	ActiveContext.Reset();
	RecentContext.Reset();
	PendingAbilityAuthorization.Reset();
}

bool FActAbilityChainRuntime::HasActiveChain() const
{
	return ActiveContext.IsValid();
}

void FActAbilityChainRuntime::BeginAbility(UActAbilitySystemComponent& ActASC, UActGameplayAbility& Ability, const FGameplayAbilitySpecHandle SpecHandle)
{
	const UActAbilityChainData* ChainData = ResolveChainData(Ability);
	if (!ChainData)
	{
		return;
	}

	RecentContext.Reset();
	ActiveContext.Reset();
	ActiveContext.AbilityInstance = &Ability;
	ActiveContext.ChainData = ChainData;
	ActiveContext.SpecHandle = SpecHandle;
	ActiveContext.AbilityID = Ability.GetAbilityID();
	ActiveContext.CurrentNodeId = ChainData->GetStartNode() ? ChainData->GetStartNode()->NodeId : NAME_None;
	ActiveContext.LastNodeChangeTime = GetCurrentTimeSeconds(ActASC);
	if (PendingAbilityAuthorization.TargetAbilityID == ActiveContext.AbilityID)
	{
		PendingAbilityAuthorization.Reset();
	}

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] Begin. Owner=%s AbilityID=%s Node=%s Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityID.ToString(),
		*ActiveContext.CurrentNodeId.ToString(),
		GetChainLogTimeSeconds(ActASC));
}

void FActAbilityChainRuntime::EndAbility(UActAbilitySystemComponent& ActASC, const UActGameplayAbility& Ability, const FGameplayAbilitySpecHandle SpecHandle)
{
	if (!ActiveContext.IsValid())
	{
		return;
	}

	const bool bMatchesAbility = ActiveContext.AbilityInstance.Get() == &Ability || ActiveContext.AbilityID == Ability.GetAbilityID();
	const bool bMatchesSpecHandle = !SpecHandle.IsValid() || ActiveContext.SpecHandle == SpecHandle;
	if (!bMatchesAbility || !bMatchesSpecHandle)
	{
		return;
	}

	const UActAbilityChainData* ChainData = ActiveContext.ChainData.Get();
	const double CurrentTimeSeconds = GetCurrentTimeSeconds(ActASC);
	const float GraceSeconds = ChainData ? ChainData->PredictionGraceSeconds : 0.0f;
	SnapshotRecentContext(CurrentTimeSeconds, GraceSeconds);

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] End. Owner=%s AbilityID=%s Node=%s Grace=%.3f Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityID.ToString(),
		*ActiveContext.CurrentNodeId.ToString(),
		GraceSeconds,
		CurrentTimeSeconds);

	ActiveContext.Reset();
}

void FActAbilityChainRuntime::EnterNode(UActAbilitySystemComponent& ActASC, const FName NodeId)
{
	if (!ActiveContext.IsValid() || NodeId.IsNone())
	{
		return;
	}

	const UActAbilityChainData* ChainData = ActiveContext.ChainData.Get();
	if (!GetNode(ChainData, NodeId))
	{
		return;
	}

	if (ActiveContext.CurrentNodeId == NodeId)
	{
		return;
	}

	ClearOpenWindowsAsClosed(GetCurrentTimeSeconds(ActASC));
	ActiveContext.CurrentNodeId = NodeId;
	ActiveContext.LastNodeChangeTime = GetCurrentTimeSeconds(ActASC);

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] EnterNode. Owner=%s AbilityID=%s Node=%s Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityID.ToString(),
		*NodeId.ToString(),
		GetChainLogTimeSeconds(ActASC));
}

void FActAbilityChainRuntime::OpenWindow(UActAbilitySystemComponent& ActASC, const FGameplayTag& WindowTag, const UObject* SourceObject)
{
	if (!ActiveContext.IsValid() || !WindowTag.IsValid() || !SourceObject)
	{
		return;
	}

	FWindowState& WindowState = ActiveContext.WindowStates.FindOrAdd(WindowTag);
	if (WindowState.Sources.Contains(SourceObject))
	{
		return;
	}

	// We track per-window "sources" so overlapping notify states do not fight each other.
	// A window is considered open as long as at least one source remains.
	WindowState.Sources.Add(SourceObject);
	WindowState.LastNodeId = ActiveContext.CurrentNodeId;
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] WindowOpen. Owner=%s AbilityID=%s Node=%s Window=%s Count=%d Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityID.ToString(),
		*ActiveContext.CurrentNodeId.ToString(),
		*WindowTag.ToString(),
		WindowState.Sources.Num(),
		GetChainLogTimeSeconds(ActASC));
}

void FActAbilityChainRuntime::CloseWindow(UActAbilitySystemComponent& ActASC, const FGameplayTag& WindowTag, const UObject* SourceObject)
{
	if (!ActiveContext.IsValid() || !WindowTag.IsValid() || !SourceObject)
	{
		return;
	}

	FWindowState* WindowState = ActiveContext.WindowStates.Find(WindowTag);
	if (!WindowState || !WindowState->Sources.Contains(SourceObject))
	{
		return;
	}

	WindowState->Sources.Remove(SourceObject);
	if (WindowState->Sources.IsEmpty())
	{
		// We keep the close time so grace seconds can still satisfy this window under weak network,
		// even if input/transition arrives slightly after the animation notify ended.
		WindowState->LastClosedTime = GetCurrentTimeSeconds(ActASC);
		WindowState->LastNodeId = ActiveContext.CurrentNodeId;
	}

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] WindowClose. Owner=%s AbilityID=%s Node=%s Window=%s Count=%d Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityID.ToString(),
		*ActiveContext.CurrentNodeId.ToString(),
		*WindowTag.ToString(),
		WindowState->Sources.Num(),
		GetChainLogTimeSeconds(ActASC));
}

bool FActAbilityChainRuntime::CanActivateAbility(const UActAbilitySystemComponent& ActASC, const UActGameplayAbility& Ability, FGameplayTagContainer* OptionalRelevantTags) const
{
	const EActAbilityChainActivationMode ActivationMode = Ability.GetAbilityChainActivationMode();
	if (ActivationMode == EActAbilityChainActivationMode::Ignore)
	{
		return true;
	}

	const FName AbilityID = Ability.GetAbilityID();
	if (AbilityID.IsNone())
	{
		return ActivationMode != EActAbilityChainActivationMode::ChainOnly;
	}

	const double CurrentTimeSeconds = GetCurrentTimeSeconds(ActASC);
	if (ActiveContext.IsValid())
	{
		if (ActivationMode == EActAbilityChainActivationMode::StarterOnly)
		{
			return false;
		}

		if (PendingAbilityAuthorization.Matches(AbilityID, CurrentTimeSeconds))
		{
			return true;
		}

		const UActAbilityChainData* ChainData = ActiveContext.ChainData.Get();
		const float GraceSeconds = ChainData ? ChainData->PredictionGraceSeconds : 0.0f;
		if (IsAbilityTransitionAllowedFromActiveContext(ActASC, AbilityID, CurrentTimeSeconds, GraceSeconds))
		{
			return true;
		}

		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.ActivateFail.Chain"), false));
		}
		return false;
	}

	if (ActivationMode != EActAbilityChainActivationMode::StarterOnly &&
		PendingAbilityAuthorization.Matches(AbilityID, CurrentTimeSeconds))
	{
		return true;
	}

	if ((ActivationMode == EActAbilityChainActivationMode::ChainOnly || ActivationMode == EActAbilityChainActivationMode::StarterOrChain) &&
		IsAbilityTransitionAllowedFromRecentContext(ActASC, AbilityID, CurrentTimeSeconds))
	{
		return true;
	}

	return ActivationMode == EActAbilityChainActivationMode::StarterOnly || ActivationMode == EActAbilityChainActivationMode::StarterOrChain;
}

FActAbilityChainCommandResolveResult FActAbilityChainRuntime::ResolveCommand(UActAbilitySystemComponent& ActASC, const FGameplayTag& CommandTag, const bool bAllowAbilityActivation)
{
	FActAbilityChainCommandResolveResult Result;
	if (!ActiveContext.IsValid() || !CommandTag.IsValid())
	{
		return Result;
	}

	const UActAbilityChainData* ChainData = ActiveContext.ChainData.Get();
	const FActAbilityChainNode* CurrentNode = GetCurrentNode();
	if (!ChainData || !CurrentNode)
	{
		return Result;
	}

	const double CurrentTimeSeconds = GetCurrentTimeSeconds(ActASC);
	const float GraceSeconds = ChainData->PredictionGraceSeconds;

	TArray<const FActAbilityChainTransition*> Candidates;
	Candidates.Reserve(CurrentNode->Transitions.Num());
	for (const FActAbilityChainTransition& Transition : CurrentNode->Transitions)
	{
		if (!Transition.CommandTag.IsValid() || Transition.CommandTag != CommandTag)
		{
			continue;
		}

		if (!SatisfiesOwnedTagRequirements(ActASC, Transition))
		{
			continue;
		}

		if (!SatisfiesWindowRequirements(Transition, CurrentTimeSeconds, GraceSeconds))
		{
			continue;
		}

		Candidates.Add(&Transition);
	}

	Candidates.Sort([](const FActAbilityChainTransition& A, const FActAbilityChainTransition& B)
	{
		return A.Priority > B.Priority;
	});

	for (const FActAbilityChainTransition* Transition : Candidates)
	{
		if (!Transition)
		{
			continue;
		}

		if (Transition->TransitionType == EActAbilityChainTransitionType::Ability &&
			!Transition->TargetAbilityID.IsNone() &&
			Transition->TargetAbilityID != ActiveContext.AbilityID)
		{
			if (!bAllowAbilityActivation)
			{
				continue;
			}

			if (Transition->bCancelCurrentAbility)
			{
				FActAbilityChainReplicatedTransition PredictedTransition;
				PredictedTransition.SourceAbilityID = ActiveContext.AbilityID;
				PredictedTransition.SourceNodeId = CurrentNode->NodeId;
				PredictedTransition.TargetAbilityID = Transition->TargetAbilityID;
				PredictedTransition.bCancelCurrentAbility = true;
				AuthorizeAbilityTransition(ActASC, PredictedTransition, CurrentTimeSeconds, GraceSeconds);
			}

			if (ActASC.TryActivateAbilityByID(Transition->TargetAbilityID, true, false, nullptr))
			{
				Result.Resolution = EActAbilityChainCommandResolution::AbilityActivated;
				Result.SourceAbilityID = ActiveContext.AbilityID;
				Result.SourceNodeId = CurrentNode->NodeId;
				Result.TargetAbilityID = Transition->TargetAbilityID;
				Result.bConsumeInput = Transition->bConsumeInput;
				Result.bCancelCurrentAbility = Transition->bCancelCurrentAbility;
				return Result;
			}

			if (Transition->bCancelCurrentAbility)
			{
				PendingAbilityAuthorization.Reset();
			}

			continue;
		}

		const FActAbilityChainNode* TargetNode = GetNode(ChainData, Transition->TargetNodeId);
		if (!TargetNode)
		{
			continue;
		}

		if (JumpToNode(ActASC, *TargetNode))
		{
			Result.Resolution = EActAbilityChainCommandResolution::SectionJump;
			Result.SourceAbilityID = ActiveContext.AbilityID;
			Result.SourceNodeId = CurrentNode->NodeId;
			Result.TargetNodeId = TargetNode->NodeId;
			Result.bConsumeInput = Transition->bConsumeInput;
			return Result;
		}
	}

	return Result;
}

bool FActAbilityChainRuntime::ApplyReplicatedTransition(UActAbilitySystemComponent& ActASC, const FActAbilityChainReplicatedTransition& Transition)
{
	if (!Transition.IsValid() || !ActiveContext.IsValid())
	{
		return false;
	}

	if (!Transition.SourceAbilityID.IsNone() && Transition.SourceAbilityID != ActiveContext.AbilityID)
	{
		return false;
	}

	if (!Transition.SourceNodeId.IsNone() && Transition.SourceNodeId != ActiveContext.CurrentNodeId)
	{
		// Stale packet guard: under bad network, predicted transitions can arrive late/out of order.
		// If server/client are no longer on the source node, applying it would "yo-yo" the combo.
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] Ignore replicated transition. Owner=%s Reason=SourceNodeMismatch CurrentNode=%s SourceNode=%s Time=%.3f"),
			*GetNameSafe(ActASC.GetAvatarActor()),
			*ActiveContext.CurrentNodeId.ToString(),
			*Transition.SourceNodeId.ToString(),
			GetChainLogTimeSeconds(ActASC));
		return false;
	}

	const UActAbilityChainData* ChainData = ActiveContext.ChainData.Get();
	if (!ChainData)
	{
		return false;
	}

	const FActAbilityChainTransition* TransitionDefinition = FindTransition(ChainData, Transition.SourceNodeId, Transition);
	if (!TransitionDefinition || !SatisfiesOwnedTagRequirements(ActASC, *TransitionDefinition))
	{
		return false;
	}

	const double CurrentTimeSeconds = GetCurrentTimeSeconds(ActASC);
	const float GraceSeconds = ChainData->PredictionGraceSeconds;
	if (Transition.IsSectionTransition())
	{
		if (Transition.TargetNodeId == ActiveContext.CurrentNodeId)
		{
			return false;
		}

		if (const FActAbilityChainNode* TargetNode = GetNode(ChainData, Transition.TargetNodeId))
		{
			return JumpToNode(ActASC, *TargetNode);
		}
		return false;
	}

	AuthorizeAbilityTransition(ActASC, Transition, CurrentTimeSeconds, GraceSeconds);
	return true;
}

FName FActAbilityChainRuntime::GetInitialSectionForAbility(const UActGameplayAbility& Ability) const
{
	const UActAbilityChainData* ChainData = ResolveChainData(Ability);
	return ChainData ? ChainData->GetStartSectionName() : NAME_None;
}

const UActAbilityChainData* FActAbilityChainRuntime::ResolveChainData(const UActGameplayAbility& Ability) const
{
	return Ability.GetAbilityChainData();
}

double FActAbilityChainRuntime::GetCurrentTimeSeconds(const UActAbilitySystemComponent& ActASC) const
{
	const UWorld* World = ActASC.GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void FActAbilityChainRuntime::SnapshotRecentContext(const double CurrentTimeSeconds, const float GraceSeconds)
{
	if (!ActiveContext.IsValid() || GraceSeconds <= 0.0f)
	{
		RecentContext.Reset();
		return;
	}

	RecentContext.Reset();
	RecentContext.ChainData = ActiveContext.ChainData;
	RecentContext.AbilityID = ActiveContext.AbilityID;
	RecentContext.CurrentNodeId = ActiveContext.CurrentNodeId;
	RecentContext.ExpiresAt = CurrentTimeSeconds + GraceSeconds;

	for (const TPair<FGameplayTag, FWindowState>& Pair : ActiveContext.WindowStates)
	{
		FWindowState SnapshotState = Pair.Value;
		if (!SnapshotState.Sources.IsEmpty())
		{
			SnapshotState.LastClosedTime = CurrentTimeSeconds;
		}

		RecentContext.LastClosedWindowStates.Add(Pair.Key, SnapshotState);
	}
}

void FActAbilityChainRuntime::ClearOpenWindowsAsClosed(const double CurrentTimeSeconds)
{
	for (TPair<FGameplayTag, FWindowState>& Pair : ActiveContext.WindowStates)
	{
		if (!Pair.Value.Sources.IsEmpty())
		{
			Pair.Value.Sources.Reset();
			Pair.Value.LastClosedTime = CurrentTimeSeconds;
			Pair.Value.LastNodeId = ActiveContext.CurrentNodeId;
		}
	}
}

const FActAbilityChainNode* FActAbilityChainRuntime::GetCurrentNode() const
{
	return GetNode(ActiveContext.ChainData.Get(), ActiveContext.CurrentNodeId);
}

const FActAbilityChainNode* FActAbilityChainRuntime::GetNode(const UActAbilityChainData* ChainData, const FName NodeId) const
{
	return ChainData ? ChainData->FindNode(NodeId) : nullptr;
}

bool FActAbilityChainRuntime::SatisfiesOwnedTagRequirements(const UActAbilitySystemComponent& ActASC, const FActAbilityChainTransition& Transition) const
{
	FGameplayTagContainer OwnedTags;
	ActASC.GetOwnedGameplayTags(OwnedTags);

	if (!Transition.BlockedOwnerTags.IsEmpty() && OwnedTags.HasAny(Transition.BlockedOwnerTags))
	{
		return false;
	}

	if (!Transition.RequiredOwnerTags.IsEmpty() && !OwnedTags.HasAll(Transition.RequiredOwnerTags))
	{
		return false;
	}

	return true;
}

bool FActAbilityChainRuntime::SatisfiesWindowRequirements(const FActAbilityChainTransition& Transition, const double CurrentTimeSeconds, const float GraceSeconds) const
{
	for (const FGameplayTag& WindowTag : Transition.RequiredWindowTags)
	{
		if (!IsWindowSatisfied(WindowTag, CurrentTimeSeconds, GraceSeconds))
		{
			return false;
		}
	}

	for (const FGameplayTag& WindowTag : Transition.BlockedWindowTags)
	{
		const FWindowState* ActiveWindowState = ActiveContext.WindowStates.Find(WindowTag);
		if (ActiveWindowState && !ActiveWindowState->Sources.IsEmpty())
		{
			return false;
		}
	}

	return true;
}

bool FActAbilityChainRuntime::IsWindowSatisfied(const FGameplayTag& WindowTag, const double CurrentTimeSeconds, const float GraceSeconds) const
{
	if (!WindowTag.IsValid())
	{
		return true;
	}

	if (const FWindowState* ActiveWindowState = ActiveContext.WindowStates.Find(WindowTag))
	{
		if (ActiveWindowState->LastNodeId != ActiveContext.CurrentNodeId)
		{
			// Prevent "window leakage" across nodes when the same tag is reused (common for combo windows).
			return false;
		}

		if (!ActiveWindowState->Sources.IsEmpty())
		{
			return true;
		}

		// Grace allows window satisfaction shortly after close time.
		// This is a key weak-network feature: client input may be buffered and released slightly late.
		if (GraceSeconds > 0.0f && CurrentTimeSeconds - ActiveWindowState->LastClosedTime <= GraceSeconds)
		{
			return true;
		}
	}

	if (RecentContext.IsValid(CurrentTimeSeconds))
	{
		if (const FWindowState* RecentWindowState = RecentContext.LastClosedWindowStates.Find(WindowTag))
		{
			if (RecentWindowState->LastNodeId != RecentContext.CurrentNodeId)
			{
				return false;
			}

			return GraceSeconds > 0.0f && CurrentTimeSeconds - RecentWindowState->LastClosedTime <= GraceSeconds;
		}
	}

	return false;
}

bool FActAbilityChainRuntime::IsAbilityTransitionAllowedFromActiveContext(
	const UActAbilitySystemComponent& ActASC,
	const FName TargetAbilityID,
	const double CurrentTimeSeconds,
	const float GraceSeconds) const
{
	const UActAbilityChainData* ChainData = ActiveContext.ChainData.Get();
	const FActAbilityChainNode* CurrentNode = GetCurrentNode();
	if (!ChainData || !CurrentNode || TargetAbilityID.IsNone())
	{
		return false;
	}

	for (const FActAbilityChainTransition& Transition : CurrentNode->Transitions)
	{
		if (Transition.TransitionType != EActAbilityChainTransitionType::Ability || Transition.TargetAbilityID != TargetAbilityID)
		{
			continue;
		}

		if (!SatisfiesOwnedTagRequirements(ActASC, Transition))
		{
			continue;
		}

		if (SatisfiesWindowRequirements(Transition, CurrentTimeSeconds, GraceSeconds))
		{
			return true;
		}
	}

	return false;
}

bool FActAbilityChainRuntime::IsAbilityTransitionAllowedFromRecentContext(
	const UActAbilitySystemComponent& ActASC,
	const FName TargetAbilityID,
	const double CurrentTimeSeconds) const
{
	if (!RecentContext.IsValid(CurrentTimeSeconds) || TargetAbilityID.IsNone())
	{
		return false;
	}

	const UActAbilityChainData* ChainData = RecentContext.ChainData.Get();
	const FActAbilityChainNode* CurrentNode = GetNode(ChainData, RecentContext.CurrentNodeId);
	if (!ChainData || !CurrentNode)
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	ActASC.GetOwnedGameplayTags(OwnedTags);

	for (const FActAbilityChainTransition& Transition : CurrentNode->Transitions)
	{
		if (Transition.TransitionType != EActAbilityChainTransitionType::Ability || Transition.TargetAbilityID != TargetAbilityID)
		{
			continue;
		}

		if (!Transition.BlockedOwnerTags.IsEmpty() && OwnedTags.HasAny(Transition.BlockedOwnerTags))
		{
			continue;
		}

		if (!Transition.RequiredOwnerTags.IsEmpty() && !OwnedTags.HasAll(Transition.RequiredOwnerTags))
		{
			continue;
		}

		bool bAllWindowsSatisfied = true;
		for (const FGameplayTag& WindowTag : Transition.RequiredWindowTags)
		{
			const FWindowState* RecentWindowState = RecentContext.LastClosedWindowStates.Find(WindowTag);
			if (!RecentWindowState || CurrentTimeSeconds > RecentContext.ExpiresAt)
			{
				bAllWindowsSatisfied = false;
				break;
			}
			if (RecentWindowState->LastNodeId != RecentContext.CurrentNodeId)
			{
				bAllWindowsSatisfied = false;
				break;
			}
		}

		if (!bAllWindowsSatisfied)
		{
			continue;
		}

		bool bAnyBlockedWindowStillHot = false;
		for (const FGameplayTag& WindowTag : Transition.BlockedWindowTags)
		{
			if (const FWindowState* RecentWindowState = RecentContext.LastClosedWindowStates.Find(WindowTag))
			{
				if (CurrentTimeSeconds <= RecentContext.ExpiresAt &&
					RecentWindowState->LastNodeId == RecentContext.CurrentNodeId)
				{
					bAnyBlockedWindowStillHot = true;
					break;
				}
			}
		}

		if (bAnyBlockedWindowStillHot)
		{
			continue;
		}

		return true;
	}

	return false;
}

const FActAbilityChainTransition* FActAbilityChainRuntime::FindTransition(
	const UActAbilityChainData* ChainData,
	const FName SourceNodeId,
	const FActAbilityChainReplicatedTransition& Transition) const
{
	const FActAbilityChainNode* SourceNode = GetNode(ChainData, SourceNodeId);
	if (!SourceNode)
	{
		return nullptr;
	}

	for (const FActAbilityChainTransition& Candidate : SourceNode->Transitions)
	{
		if (Transition.IsSectionTransition())
		{
			if (Candidate.TransitionType == EActAbilityChainTransitionType::Section &&
				Candidate.TargetNodeId == Transition.TargetNodeId)
			{
				return &Candidate;
			}
			continue;
		}

		if (Candidate.TransitionType == EActAbilityChainTransitionType::Ability &&
			Candidate.TargetAbilityID == Transition.TargetAbilityID)
		{
			return &Candidate;
		}
	}

	return nullptr;
}

void FActAbilityChainRuntime::AuthorizeAbilityTransition(
	UActAbilitySystemComponent& ActASC,
	const FActAbilityChainReplicatedTransition& Transition,
	const double CurrentTimeSeconds,
	const float GraceSeconds)
{
	PendingAbilityAuthorization.SourceAbilityID = Transition.SourceAbilityID;
	PendingAbilityAuthorization.SourceNodeId = Transition.SourceNodeId;
	PendingAbilityAuthorization.TargetAbilityID = Transition.TargetAbilityID;
	PendingAbilityAuthorization.ExpiresAt = CurrentTimeSeconds + GraceSeconds;

	if (Transition.bCancelCurrentAbility && ActiveContext.SpecHandle.IsValid())
	{
		ActASC.CancelAbilityByHandle(ActiveContext.SpecHandle);
	}

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] AuthorizeAbility. Owner=%s SourceAbility=%s SourceNode=%s TargetAbility=%s ExpiresAt=%.3f Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*Transition.SourceAbilityID.ToString(),
		*Transition.SourceNodeId.ToString(),
		*Transition.TargetAbilityID.ToString(),
		PendingAbilityAuthorization.ExpiresAt,
		CurrentTimeSeconds);
}

bool FActAbilityChainRuntime::JumpToNode(UActAbilitySystemComponent& ActASC, const FActAbilityChainNode& TargetNode)
{
	if (TargetNode.NodeId.IsNone() || TargetNode.SectionName.IsNone())
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = ActASC.AbilityActorInfo.Get();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentMontage = ActASC.GetCurrentMontage();
	if (!AnimInstance || !CurrentMontage || !CurrentMontage->IsValidSectionName(TargetNode.SectionName))
	{
		return false;
	}

	ClearOpenWindowsAsClosed(GetCurrentTimeSeconds(ActASC));
	AnimInstance->Montage_JumpToSection(TargetNode.SectionName, CurrentMontage);
	ActiveContext.CurrentNodeId = TargetNode.NodeId;
	ActiveContext.LastNodeChangeTime = GetCurrentTimeSeconds(ActASC);

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[AbilityChain] JumpToNode. Owner=%s AbilityID=%s Node=%s Section=%s Time=%.3f"),
		*GetNameSafe(ActASC.GetAvatarActor()),
		*ActiveContext.AbilityID.ToString(),
		*TargetNode.NodeId.ToString(),
		*TargetNode.SectionName.ToString(),
		GetChainLogTimeSeconds(ActASC));

	return true;
}
