// BulletSystem: BulletBudgetComponent.cpp
// Components used by bullet entities/actors.
#include "Component/BulletBudgetComponent.h"

void UBulletBudgetComponent::Initialize(float InInterval, float SpawnTime)
{
    (void)SpawnTime;
    BudgetInterval = InInterval;
    LastMoveTickTime = -BIG_NUMBER;
    LastCollisionTickTime = -BIG_NUMBER;
}

void UBulletBudgetComponent::Reset()
{
    Super::Reset();
    BudgetInterval = 0.0f;
    LastMoveTickTime = -BIG_NUMBER;
    LastCollisionTickTime = -BIG_NUMBER;
}

bool UBulletBudgetComponent::ConsumeMoveTick(float WorldTime, float DefaultDelta, float& OutDelta)
{
    if (BudgetInterval <= 0.0f)
    {
        OutDelta = DefaultDelta;
        return true;
    }

    if ((WorldTime - LastMoveTickTime) < BudgetInterval)
    {
        return false;
    }

    OutDelta = (LastMoveTickTime <= -BIG_NUMBER * 0.5f) ? DefaultDelta : (WorldTime - LastMoveTickTime);
    LastMoveTickTime = WorldTime;
    return true;
}

bool UBulletBudgetComponent::ConsumeCollisionTick(float WorldTime, float DefaultDelta, float& OutDelta)
{
    if (BudgetInterval <= 0.0f)
    {
        OutDelta = DefaultDelta;
        return true;
    }

    if ((WorldTime - LastCollisionTickTime) < BudgetInterval)
    {
        return false;
    }

    OutDelta = (LastCollisionTickTime <= -BIG_NUMBER * 0.5f) ? DefaultDelta : (WorldTime - LastCollisionTickTime);
    LastCollisionTickTime = WorldTime;
    return true;
}

