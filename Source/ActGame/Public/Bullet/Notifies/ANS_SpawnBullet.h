// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "BulletSystemTypes.h"

#include "ANS_SpawnBullet.generated.h"

/**
 * Spawn bullet during a window, auto-destroy at NotifyEnd.
 *
 * Uses an editor-stable NotifyId per notify instance, so GA can map begin/end to the correct bullet instance
 * without manual slot configuration.
 */
UCLASS(meta = (DisplayName = "Spawn Bullet (State)"))
class ACTGAME_API UANS_SpawnBullet : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName BulletID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag SpawnEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag EndEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|End")
	EBulletDestroyReason EndReason = EBulletDestroyReason::External;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|End")
	bool bSpawnChildrenOnEnd = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|SimProxy")
	bool bForceCollisionOnSimProxy = true;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

private:
	// Unique ID per notify instance for runtime mapping.
	UPROPERTY(VisibleAnywhere, Category = "Bullet")
	FName NotifyId;
};
