// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"

#include "AN_ProcessManualHits.generated.h"

/**
 * Process manual-hit bullets (hitbox) AnimNotify.
 * Thin forwarder into UActBulletComponent.
 */
UCLASS(meta = (DisplayName = "Process Manual Hits"))
class ACTGAME_API UAN_ProcessManualHits : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet", meta = (ClampMin = "-1"))
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	bool bResetHitActorsBefore = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	bool bApplyCollisionResponse = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

