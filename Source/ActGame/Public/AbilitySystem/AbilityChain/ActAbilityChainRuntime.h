#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"

#include "AbilitySystem/AbilityChain/ActAbilityChainTypes.h"

class UActAbilityChainData;
class UActAbilitySystemComponent;
class UActGameplayAbility;
struct FActAbilityChainNode;
struct FActAbilityChainTransition;

enum class EActAbilityChainCommandResolution : uint8
{
	NotHandled,
	SectionJump,
	AbilityActivated
};

/**
 * Resolution result returned to the input layer after evaluating a command inside an active chain.
 *
 * The input system should treat this as an opaque decision:
 * - It may consume the command from buffer (bConsumeInput).
 * - It may need to relay a predicted transition to server (ToReplicatedTransition()).
 */
struct FActAbilityChainCommandResolveResult
{
	EActAbilityChainCommandResolution Resolution = EActAbilityChainCommandResolution::NotHandled;
	FName SourceAbilityID = NAME_None;
	FName SourceNodeId = NAME_None;
	FName TargetAbilityID = NAME_None;
	FName TargetNodeId = NAME_None;
	bool bConsumeInput = true;
	bool bCancelCurrentAbility = false;

	/** True if this command resulted in an actionable chain decision. */
	bool WasHandled() const
	{
		return Resolution != EActAbilityChainCommandResolution::NotHandled;
	}

	/** Converts to a lightweight replicated transition payload (used for client -> server relay). */
	FActAbilityChainReplicatedTransition ToReplicatedTransition() const
	{
		FActAbilityChainReplicatedTransition Transition;
		Transition.SourceAbilityID = SourceAbilityID;
		Transition.SourceNodeId = SourceNodeId;
		Transition.TargetAbilityID = TargetAbilityID;
		Transition.TargetNodeId = TargetNodeId;
		Transition.bCancelCurrentAbility = bCancelCurrentAbility;
		return Transition;
	}
};

/**
 * Runtime combo/ability-chain state machine living inside UActAbilitySystemComponent.
 *
 * Goals:
 * - Keep chain authority in ASC (not in animation notifies).
 * - Support LocalPredicted gameplay: client predicts node jumps/ability transitions, server confirms.
 * - Provide "window" semantics via AnimNotifyState signals, with optional grace seconds for weak network.
 *
 * Key concepts:
 * - Node: a logical step in a combo, typically 1:1 with a montage section.
 * - Window: a tag-authored time range during which certain transitions are allowed.
 * - Transition: a rule from current node to target node or target ability, gated by command + windows + tags.
 */
class ACTGAME_API FActAbilityChainRuntime
{
public:
	FActAbilityChainRuntime() = default;

	/** Clears active/recent state and pending authorization. */
	void Reset();
	/** True if there is an active chain context (i.e. we are inside an ability with chain data). */
	bool HasActiveChain() const;

	/** Called when an ability that owns a chain begins; initializes ActiveContext. */
	void BeginAbility(UActAbilitySystemComponent& ActASC, UActGameplayAbility& Ability, FGameplayAbilitySpecHandle SpecHandle);
	/** Called when that ability ends; snapshots RecentContext for grace and clears ActiveContext. */
	void EndAbility(UActAbilitySystemComponent& ActASC, const UActGameplayAbility& Ability, FGameplayAbilitySpecHandle SpecHandle);
	/** Advances current node when animation notifies report entering a new node/section. */
	void EnterNode(UActAbilitySystemComponent& ActASC, FName NodeId);
	/** Opens a window tag (source object is used to handle overlapping notifies). */
	void OpenWindow(UActAbilitySystemComponent& ActASC, const FGameplayTag& WindowTag, const UObject* SourceObject);
	/** Closes a window tag. */
	void CloseWindow(UActAbilitySystemComponent& ActASC, const FGameplayTag& WindowTag, const UObject* SourceObject);

	/**
	 * Activation gate used by ASC to decide whether an ability may activate right now.
	 * This is what prevents random abilities from being started during a chain unless authorized.
	 */
	bool CanActivateAbility(const UActAbilitySystemComponent& ActASC, const UActGameplayAbility& Ability, FGameplayTagContainer* OptionalRelevantTags) const;

	/**
	 * Evaluate one command against the current node transitions.
	 * If bAllowAbilityActivation is false, ability transitions will be ignored (used by certain input phases).
	 */
	FActAbilityChainCommandResolveResult ResolveCommand(UActAbilitySystemComponent& ActASC, const FGameplayTag& CommandTag, bool bAllowAbilityActivation);

	/**
	 * Applies a replicated transition (typically server -> client catch-up).
	 * We reject stale transitions by validating SourceNodeId against current node.
	 */
	bool ApplyReplicatedTransition(UActAbilitySystemComponent& ActASC, const FActAbilityChainReplicatedTransition& Transition);

	/** Returns the start montage section name for a chain ability (used when StartSection is left empty). */
	FName GetInitialSectionForAbility(const UActGameplayAbility& Ability) const;

private:
	struct FWindowState
	{
		TSet<const UObject*> Sources;
		double LastClosedTime = TNumericLimits<double>::Lowest();
		FName LastNodeId = NAME_None;
	};

	struct FActiveChainContext
	{
		TWeakObjectPtr<UActGameplayAbility> AbilityInstance;
		TWeakObjectPtr<const UActAbilityChainData> ChainData;
		FGameplayAbilitySpecHandle SpecHandle;
		FName AbilityID;
		FName CurrentNodeId;
		TMap<FGameplayTag, FWindowState> WindowStates;
		double LastNodeChangeTime = 0.0;

		void Reset();
		bool IsValid() const;
	};

	struct FRecentChainContext
	{
		TWeakObjectPtr<const UActAbilityChainData> ChainData;
		FName AbilityID;
		FName CurrentNodeId;
		TMap<FGameplayTag, FWindowState> LastClosedWindowStates;
		double ExpiresAt = 0.0;

		void Reset();
		bool IsValid(double CurrentTimeSeconds) const;
	};

	struct FPendingAbilityAuthorization
	{
		FName SourceAbilityID;
		FName SourceNodeId;
		FName TargetAbilityID;
		double ExpiresAt = 0.0;

		void Reset();
		bool Matches(FName InAbilityID, double CurrentTimeSeconds) const;
	};

private:
	const UActAbilityChainData* ResolveChainData(const UActGameplayAbility& Ability) const;
	double GetCurrentTimeSeconds(const UActAbilitySystemComponent& ActASC) const;
	void SnapshotRecentContext(double CurrentTimeSeconds, float GraceSeconds);
	void ClearOpenWindowsAsClosed(double CurrentTimeSeconds);
	const FActAbilityChainNode* GetCurrentNode() const;
	const FActAbilityChainNode* GetNode(const UActAbilityChainData* ChainData, FName NodeId) const;
	bool SatisfiesOwnedTagRequirements(const UActAbilitySystemComponent& ActASC, const FActAbilityChainTransition& Transition) const;
	bool SatisfiesWindowRequirements(const FActAbilityChainTransition& Transition, double CurrentTimeSeconds, float GraceSeconds) const;
	bool IsWindowSatisfied(const FGameplayTag& WindowTag, double CurrentTimeSeconds, float GraceSeconds) const;
	bool IsAbilityTransitionAllowedFromActiveContext(const UActAbilitySystemComponent& ActASC, FName TargetAbilityID, double CurrentTimeSeconds, float GraceSeconds) const;
	bool IsAbilityTransitionAllowedFromRecentContext(const UActAbilitySystemComponent& ActASC, FName TargetAbilityID, double CurrentTimeSeconds) const;
	const FActAbilityChainTransition* FindTransition(const UActAbilityChainData* ChainData, FName SourceNodeId, const FActAbilityChainReplicatedTransition& Transition) const;
	void AuthorizeAbilityTransition(UActAbilitySystemComponent& ActASC, const FActAbilityChainReplicatedTransition& Transition, double CurrentTimeSeconds, float GraceSeconds);
	bool JumpToNode(UActAbilitySystemComponent& ActASC, const FActAbilityChainNode& TargetNode);

private:
	FActiveChainContext ActiveContext;
	FRecentChainContext RecentContext;
	FPendingAbilityAuthorization PendingAbilityAuthorization;
};
