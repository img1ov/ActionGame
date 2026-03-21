// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "BulletSystemTypes.h"

#include "AN_SpawnBulletSimProxy.generated.h"

/**
 * Spawn a BulletSystem bullet for simulated proxies when their montage hits the notify frame.
 *
 * Why:
 * - GAS abilities do not run on simulated proxies, but montages replicate and notifies still fire.
 * - This notify fills the gap so remote clients can see projectile VFX/trajectory aligned to the montage.
 *
 * Notes:
 * - This is cosmetic-only from a networking standpoint. Authoritative damage should still be server-side
 *   (e.g. via BulletSystem LogicData_ApplyGameplayEffect with bApplyOnServerOnly).
 * - For aimed bullets that require runtime target data, you will likely need an additional replicated
 *   "attack context" (target actor/location or aim direction) for accurate visuals.
 */
UCLASS(meta = (DisplayName = "Spawn Bullet (SimProxy)"))
class ACTGAME_API UAN_SpawnBulletSimProxy : public UAnimNotify
{
	GENERATED_BODY()

public:
	// Bullet ID defined in BulletConfig (DataTable/asset).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName BulletID;

	// Optional socket on the skeletal mesh to use as spawn transform source.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName SpawnSocketName;

	// Additional per-shot offset applied by BulletSystem (in world space unless config uses owner space).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FVector SpawnOffset = FVector::ZeroVector;

	// If false, force collision disabled (visual-only) for the spawned bullet.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	bool bEnableCollision = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

