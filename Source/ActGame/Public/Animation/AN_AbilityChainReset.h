// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "AN_AbilityChainReset.generated.h"

UCLASS(meta = (DisplayName = "Ability Chain Reset"))
class ACTGAME_API UAN_AbilityChainReset : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
