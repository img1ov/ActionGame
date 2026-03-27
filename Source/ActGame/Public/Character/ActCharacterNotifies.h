#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "ActCharacterNotifies.generated.h"

/**
 * Pure forwarding notify state for movement-input blocking.
 *
 * Business logic remains inside UActHeroComponent. The notify only opens/closes one source token.
 */
UCLASS(meta = (DisplayName = "Act Movement Input Block"))
class ACTGAME_API UANS_MovementInputBlock : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
