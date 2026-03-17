#pragma once

// BulletSystem: BulletMoveSystem.h
// High-frequency simulation systems (move/collision).

#include "CoreMinimal.h"
#include "System/BulletSystemBase.h"
#include "BulletMoveSystem.generated.h"

UCLASS()
class BULLETGAME_API UBulletMoveSystem : public UBulletSystemBase
{
    GENERATED_BODY()

public:
    // High-frequency movement update system.
    // This updates FBulletMoveInfo for all active bullets and then syncs the render actor transform (if any).
    // Bullet logic may hook into pre/post move or fully replace movement (see UBulletActionLogicComponent).
    virtual void OnTick(float DeltaSeconds) override;
};

