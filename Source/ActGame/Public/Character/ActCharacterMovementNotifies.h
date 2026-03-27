#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "ActCharacterMovementNotifies.generated.h"

/**
 * Opens a generic cancel window on the character movement side.
 *
 * Design intent:
 * - This notify does NOT block locomotion input.
 * - It only marks the authored animation range as "cancelable" so gameplay code
 *   can decide to cancel abilities (move-cancel, dodge-cancel, skill-cancel, etc).
 *
 * Current implementation stores the window on UActCharacterMovementComponent as a
 * set of active sources (the notify instance pointer is used as a source key).
 */
UCLASS(meta = (DisplayName = "Act Cancel Window"))
class ACTGAME_API UANS_CancelWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** Opens the cancel window for this mesh owner's movement component. */
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	/** Closes the cancel window for this mesh owner's movement component. */
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
