#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "AbilitySystem/AbilityChain/ActAbilityChainTypes.h"
#include "ActAbilityChainNotifies.generated.h"

/**
 * Window-authored combo notify state used by the upgraded first-generation combo runtime.
 *
 * Each notify carries all follow-up authoring it needs. The runtime simply registers the
 * window on Begin and unregisters it on End.
 */
UCLASS(meta = (DisplayName = "Act AbilityChain Window"))
class ACTGAME_API UANS_AbilityChainWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** Registers this authored combo window into the owner's ASC runtime. */
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	/** Unregisters this authored combo window from the owner's ASC runtime. */
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** Returns the fully-authored window definition that should be registered at runtime. */
	FActAbilityChainWindowDefinition BuildWindowDefinition() const;

	/** Returns a deterministic runtime window Id for this authored notify instance. */
	FName GetWindowId() const;

protected:
	/** Higher value wins when this window overlaps another combo window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (ClampMin = "0"))
	int32 WindowPriority = 0;

	/** Follow-up entries available while this window is open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (TitleProperty = "TargetAbilityId"))
	TArray<FActAbilityChainEntry> Entries;
};
