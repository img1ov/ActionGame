#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"

#include "AbilitySystem/AbilityChain/ActAbilityChainTypes.h"

class UActAbilitySystemComponent;
class UActGameplayAbility;

enum class EActAbilityChainCommandResolution : uint8
{
	NotHandled,
	AbilityActivated
};

/**
 * Resolution result returned to the input layer after evaluating one command.
 *
 * The input layer only needs to know whether the command produced a successful activation
 * and whether the command buffer should be consumed afterwards.
 */
struct FActAbilityChainCommandResolveResult
{
	/** Final runtime outcome for the command. */
	EActAbilityChainCommandResolution Resolution = EActAbilityChainCommandResolution::NotHandled;

	/** Source active ability that owned the winning combo windows. */
	FName SourceAbilityId = NAME_None;

	/** Follow-up ability activated by the command. */
	FName TargetAbilityId = NAME_None;

	/** True when the command should be removed from the analyzer buffer after handling. */
	bool bConsumeInput = true;

	/** True if this command resulted in an actionable chain decision. */
	bool WasHandled() const
	{
		return Resolution != EActAbilityChainCommandResolution::NotHandled;
	}
};

/**
 * Runtime combo state machine living inside UActAbilitySystemComponent.
 *
 * This runtime implements the upgraded first-generation design:
 * - authoring lives entirely on AnimNotifyState combo windows
 * - the ASC owns all authoritative runtime state
 * - the client may predict a follow-up ability, but the server still validates the request
 */
class ACTGAME_API FActAbilityChainRuntime
{
public:
	FActAbilityChainRuntime() = default;

	/** Clears active state, registered windows, and any pending follow-up authorization. */
	void Reset();

	/** True if there is a currently active ability that still owns valid combo runtime state. */
	bool HasActiveChain(const UActAbilitySystemComponent& ActASC) const;

	/** Registers a new active combo source ability. */
	void BeginAbility(UActAbilitySystemComponent& ActASC, UActGameplayAbility& Ability, FGameplayAbilitySpecHandle SpecHandle);

	/** Clears the active combo source if the specified ability was the current owner. */
	void EndAbility(UActAbilitySystemComponent& ActASC, const UActGameplayAbility& Ability, FGameplayAbilitySpecHandle SpecHandle);

	/** Opens or refreshes one authored combo window. */
	void OpenWindow(UActAbilitySystemComponent& ActASC, const FActAbilityChainWindowDefinition& WindowDefinition);

	/** Closes one authored combo window by its stable window Id. */
	void CloseWindow(UActAbilitySystemComponent& ActASC, FName WindowId);

	/**
	 * Activation gate used by ASC to decide whether an ability may activate while a combo source is active.
	 *
	 * During an active combo, only the currently authorized follow-up ability may start.
	 */
	bool CanActivateAbility(const UActAbilitySystemComponent& ActASC, const UActGameplayAbility& Ability, FGameplayTagContainer* OptionalRelevantTags) const;

	/**
	 * Evaluates one command against all currently active combo windows.
	 *
	 * The runtime follows the first-generation precedence rules:
	 * - sort overlapping windows by WindowPriority descending
	 * - inside each window, choose the highest-priority matching entry
	 * - attempt activation; if it fails, continue to the next window
	 */
	FActAbilityChainCommandResolveResult ResolveCommand(UActAbilitySystemComponent& ActASC, const FGameplayTag& CommandTag);

	/**
	 * Validates and authorizes one client-predicted follow-up request on the server.
	 *
	 * This does not activate the target ability by itself; it only opens the server-side gate
	 * so the subsequent GAS activation request may pass CanActivateAbility.
	 */
	bool AuthorizePredictedActivation(UActAbilitySystemComponent& ActASC, const FActAbilityChainActivationRequest& Request);

	/** Clears one pending follow-up authorization after an activation failure. */
	void NotifyAbilityActivationFailed(FName AbilityId);

private:
	/** Mutable runtime state for one authored combo window. */
	struct FWindowState
	{
		/** Static authored contents copied from the notify when it opens. */
		FActAbilityChainWindowDefinition Definition;

		/** Number of active begin/end scopes currently holding the window open. */
		int32 OpenCount = 0;

		/** Monotonic sequence used for deterministic tie-breaking between same-priority windows. */
		uint64 OpenSequence = 0;

		/** Time when the window last fully closed. Used for weak-network server validation grace. */
		double LastClosedTimeSeconds = TNumericLimits<double>::Lowest();

		/** Returns true if the window is currently open. */
		bool IsOpen() const { return OpenCount > 0; }
	};

	/** The active combo source ability currently owning all registered windows. */
	struct FActiveChainContext
	{
		/** Instanced ability object currently driving combo notifies. */
		TWeakObjectPtr<UActGameplayAbility> AbilityInstance;

		/** Spec handle of the source ability. */
		FGameplayAbilitySpecHandle SpecHandle;

		/** Stable authored Id of the source ability. */
		FName AbilityId;

		/** Clears the source ability context. */
		void Reset();

		/** True if the source ability context can drive combo windows. */
		bool IsValid() const;
	};

	/** Pending authorization for one follow-up ability predicted by the client and/or accepted by the server. */
	struct FPendingActivationAuthorization
	{
		/** Source ability that produced the authorization. */
		FName SourceAbilityId;

		/** Prediction key of the source ability activation that produced the authorization. */
		int16 SourceActivationPredictionKey = 0;

		/** Command that produced the authorization. Stored for deduplication and diagnostics. */
		FGameplayTag CommandTag;

		/** Follow-up ability currently allowed to activate. */
		FName TargetAbilityId;

		/** Spec handle of the source ability that should be canceled after follow-up activation succeeds. */
		FGameplayAbilitySpecHandle SourceSpecHandle;

		/** Creation time for diagnostics and stable ordering. */
		double AuthorizedAtSeconds = 0.0;

		/** Expiration time for the authorization gate. */
		double ExpiresAtSeconds = 0.0;

		/** Clears the authorization gate. */
		void Reset();

		/** True if this authorization has already expired. */
		bool IsExpired(double CurrentTimeSeconds) const;

		/** True if the specified ability is still authorized to activate. */
		bool MatchesTarget(FName AbilityId, double CurrentTimeSeconds) const;

		/** True if this authorization already represents the same predicted follow-up request. */
		bool MatchesRequest(const FActAbilityChainActivationRequest& Request, FGameplayAbilitySpecHandle InSourceSpecHandle, double CurrentTimeSeconds) const;
	};

private:
	/** Returns the current world time for grace / expiration calculations. */
	double GetCurrentTimeSeconds(const UActAbilitySystemComponent& ActASC) const;

	/** Returns the prediction key of the currently active combo source ability, if any. */
	int16 GetActiveContextPredictionKey() const;

	/** True if the cached combo source spec still exists and remains active on the ASC. */
	bool HasUsableActiveContext(const UActAbilitySystemComponent& ActASC) const;

	/** Resets the combo runtime if the cached combo source spec is no longer valid. */
	bool ResetIfActiveContextInvalid(UActAbilitySystemComponent& ActASC);

	/** Clears all registered window state without touching the active source ability. */
	void ResetWindowStates();

	/** True if the specified runtime window may still satisfy server validation. */
	bool IsWindowAvailableForValidation(const FWindowState& WindowState, double CurrentTimeSeconds) const;

	/** Builds a deterministic list of candidate windows for the given command. */
	void GatherCandidateWindows(const FGameplayTag& CommandTag, double CurrentTimeSeconds, bool bAllowValidationGrace, TArray<const FWindowState*>& OutWindows) const;

	/** Returns the highest-priority matching entry inside one specific window. */
	const FActAbilityChainEntry* FindBestMatchingEntryInWindow(const FWindowState& WindowState, const FGameplayTag& CommandTag) const;

	/** True if the active runtime currently permits the specified command -> ability pair. */
	bool IsAuthorizedRequest(const FActAbilityChainActivationRequest& Request, double CurrentTimeSeconds) const;

	/** Drops expired follow-up authorizations before making a new runtime decision. */
	void PruneExpiredPendingActivations(double CurrentTimeSeconds);

	/** Opens the local activation gate and remembers which source ability should be canceled after success. */
	void GrantPendingActivation(const FActAbilityChainActivationRequest& Request, double CurrentTimeSeconds);

	/** True if there is at least one unexpired authorization for the specified follow-up ability. */
	bool HasPendingActivationForAbility(FName AbilityId, double CurrentTimeSeconds) const;

	/** Finds the newest unexpired authorization that can activate the specified follow-up ability. */
	int32 FindPendingActivationIndexByAbility(FName AbilityId, double CurrentTimeSeconds) const;

	/** Finds an equivalent authorization so repeated weak-network requests refresh instead of stacking. */
	int32 FindPendingActivationIndexByRequest(const FActAbilityChainActivationRequest& Request, FGameplayAbilitySpecHandle SourceSpecHandle, double CurrentTimeSeconds) const;

	/** Removes one authorization only if it targets the specified follow-up ability. */
	void ClearPendingActivationIfMatches(FName AbilityId);

	/** Cancels the old source ability after the follow-up ability has started successfully. */
	void CancelPreviousSourceAbilityIfNeeded(UActAbilitySystemComponent& ActASC, FGameplayAbilitySpecHandle NewSpecHandle, FName NewAbilityId);

	/** Attempts to activate one follow-up ability from the currently active combo source. */
	bool TryActivateChainEntry(UActAbilitySystemComponent& ActASC, const FGameplayTag& CommandTag, const FActAbilityChainEntry& Entry);

private:
	/** Current combo source ability. */
	FActiveChainContext ActiveContext;

	/** Runtime state for each currently known combo window, keyed by stable WindowId. */
	TMap<FName, FWindowState> WindowStates;

	/** Pending client/server follow-up authorizations kept alive long enough to tolerate weak network jitter. */
	TArray<FPendingActivationAuthorization> PendingActivations;

	/** Monotonic open sequence used to break same-priority window ties deterministically. */
	uint64 NextWindowOpenSequence = 0;
};
