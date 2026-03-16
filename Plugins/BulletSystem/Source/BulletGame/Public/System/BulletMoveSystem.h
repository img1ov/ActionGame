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
    virtual void OnTick(float DeltaSeconds) override;
};

