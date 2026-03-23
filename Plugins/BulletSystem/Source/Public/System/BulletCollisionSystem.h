#pragma once

// BulletSystem: BulletCollisionSystem.h
// High-frequency simulation systems (move/collision).

#include "CoreMinimal.h"
#include "System/BulletSystemBase.h"
#include "BulletCollisionSystem.generated.h"

UCLASS()
class BULLETSYSTEM_API UBulletCollisionSystem : public UBulletSystemBase
{
    GENERATED_BODY()

public:
    // High-frequency collision and hit processing system.
    // Produces overlaps/hits based on the configured collision mode and forwards results to controller logic.
    virtual void OnTick(float DeltaSeconds) override;
};

