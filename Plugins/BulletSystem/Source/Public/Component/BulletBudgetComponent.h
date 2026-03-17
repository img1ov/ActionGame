#pragma once

// BulletSystem: BulletBudgetComponent.h
// Components used by bullet entities/actors.

#include "CoreMinimal.h"
#include "Component/BulletEntityComponent.h"
#include "BulletBudgetComponent.generated.h"

UCLASS()
class BULLETGAME_API UBulletBudgetComponent : public UBulletEntityComponent
{
    GENERATED_BODY()

public:
    // Initializes budgeting with a minimum interval between ticks (seconds).
    // Interval <= 0 disables budgeting (tick every frame).
    void Initialize(float InInterval, float SpawnTime);
    virtual void Reset() override;

    // Returns true if movement tick should run this frame. OutDelta is adjusted to the time since last tick.
    bool ConsumeMoveTick(float WorldTime, float DefaultDelta, float& OutDelta);
    // Returns true if collision tick should run this frame. OutDelta is adjusted to the time since last tick.
    bool ConsumeCollisionTick(float WorldTime, float DefaultDelta, float& OutDelta);

private:
    // Minimum interval between ticks (seconds). Shared by movement and collision (tracked separately).
    float BudgetInterval = 0.0f;
    float LastMoveTickTime = -BIG_NUMBER;
    float LastCollisionTickTime = -BIG_NUMBER;
};

