#include "System/BulletMoveSystem.h"
#include "Controller/BulletController.h"
#include "Model/BulletModel.h"
#include "Model/BulletInfo.h"
#include "Actor/BulletActor.h"
#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"
#include "Component/BulletBudgetComponent.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveVector.h"

void UBulletMoveSystem::OnTick(float DeltaSeconds)
{
    if (!Controller)
    {
        return;
    }

    UBulletModel* Model = Controller->GetModel();
    if (!Model)
    {
        return;
    }

    const float WorldTime = Controller->GetWorldTimeSeconds();

    // Snapshot ids to keep iteration stable if logic spawns bullets mid-tick (avoids iterator invalidation / rehash).
    TArray<int32> BulletIds;
    BulletIds.Reserve(Model->GetBulletMap().Num());
    for (const auto& Pair : Model->GetBulletMap())
    {
        BulletIds.Add(Pair.Key);
    }

    for (int32 BulletId : BulletIds)
    {
        FBulletInfo* InfoPtr = Model->GetBullet(BulletId);
        if (!InfoPtr)
        {
            continue;
        }
        FBulletInfo& Info = *InfoPtr;
        if (!Info.bIsInit || Info.bNeedDestroy)
        {
            continue;
        }

        if (Info.bFrozen || (Info.FrozenUntilTime > 0.0f && Info.FrozenUntilTime > WorldTime))
        {
            continue;
        }

        float RawDelta = DeltaSeconds;
        if (Info.Entity && Info.Entity->GetBudgetComponent())
        {
            // Budgeting can decimate updates for cheap bullets (e.g. off-screen / low priority).
            // OutDelta is adjusted to reflect time since last tick to keep movement roughly consistent.
            if (!Info.Entity->GetBudgetComponent()->ConsumeMoveTick(WorldTime, DeltaSeconds, RawDelta))
            {
                continue;
            }
        }
        const float ScaledDelta = RawDelta * Info.TimeScale;
        Info.MoveInfo.LastLocation = Info.MoveInfo.Location;

        bool bSkipDefaultMove = false;
        if (!Info.bIsSimple && Info.Entity && Info.Entity->GetLogicComponent())
        {
            // Allow logic to react before we move (e.g. steering, state updates).
            Info.Entity->GetLogicComponent()->HandlePreMove(Info, ScaledDelta);
            // Logic can fully replace movement. We still tick logic and sync actor transform below.
            bSkipDefaultMove = Info.Entity->GetLogicComponent()->HandleReplaceMove(Info, ScaledDelta);
        }

        const FBulletDataMove& MoveData = Info.Config.Move;

        // If movement is replaced by logic and we aren't running a custom curve, keep location as-is and only tick logic.
        if (bSkipDefaultMove && !Info.MoveInfo.CustomMoveCurve)
        {
            if (Info.Actor)
            {
                Info.Actor->SetActorLocationAndRotation(Info.MoveInfo.Location, Info.MoveInfo.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
            }

            if (!Info.bIsSimple && Info.Entity && Info.Entity->GetLogicComponent())
            {
                Info.Entity->GetLogicComponent()->TickLogic(Info, ScaledDelta);
                Info.Entity->GetLogicComponent()->HandlePostMove(Info, ScaledDelta);
            }
            continue;
        }

        if (Info.MoveInfo.bAttachToOwner && Info.InitParams.Owner)
        {
            // "Hard attach" for bullets that should follow the owner every tick (e.g. melee hitboxes).
            Info.MoveInfo.Location = Info.InitParams.Owner->GetActorLocation();
            Info.MoveInfo.Rotation = Info.InitParams.Owner->GetActorRotation();
        }
        else if (Info.MoveInfo.CustomMoveCurve && Info.MoveInfo.CustomMoveDuration > 0.0f)
        {
            // Curve-driven movement: interpret the curve output as an offset from the cached start location.
            Info.MoveInfo.CustomMoveElapsed += ScaledDelta;
            const float Alpha = FMath::Clamp(Info.MoveInfo.CustomMoveElapsed / Info.MoveInfo.CustomMoveDuration, 0.0f, 1.0f);
            const FVector CurveOffset = Info.MoveInfo.CustomMoveCurve->GetVectorValue(Alpha);
            // Curve offsets are evaluated relative to the cached start location.
            Info.MoveInfo.Location = Info.MoveInfo.CustomMoveStartLocation + CurveOffset;
            const FVector Delta = Info.MoveInfo.Location - Info.MoveInfo.LastLocation;
            if (!Delta.IsNearlyZero())
            {
                Info.MoveInfo.Rotation = Delta.ToOrientationRotator();
            }
        }
        else
        {
            switch (MoveData.MoveType)
            {
            case EBulletMoveType::Orbit:
            {
                if (!Info.MoveInfo.bOrbitCenterInitialized && Info.InitParams.Owner)
                {
                    // Cache orbit center once on first tick to avoid jitter if owner moves.
                    Info.MoveInfo.OrbitCenter = Info.InitParams.Owner->GetActorLocation();
                    Info.MoveInfo.bOrbitCenterInitialized = true;
                }
                Info.MoveInfo.OrbitAngle += MoveData.OrbitAngularSpeed * ScaledDelta;
                const float Radius = MoveData.OrbitRadius;
                const float AngleRad = FMath::DegreesToRadians(Info.MoveInfo.OrbitAngle);
                Info.MoveInfo.Location = Info.MoveInfo.OrbitCenter + FVector(FMath::Cos(AngleRad) * Radius, FMath::Sin(AngleRad) * Radius, 0.0f);
                break;
            }
            case EBulletMoveType::FollowTarget:
            {
                // Steering directly toward the target actor each tick.
                if (Info.InitParams.TargetActor)
                {
                    const FVector TargetLocation = Info.InitParams.TargetActor->GetActorLocation();
                    const FVector Direction = (TargetLocation - Info.MoveInfo.Location).GetSafeNormal();
                    Info.MoveInfo.Velocity = Direction * MoveData.Speed;
                }
                Info.MoveInfo.Location += Info.MoveInfo.Velocity * ScaledDelta;
                break;
            }
            case EBulletMoveType::Parabola:
            {
                // Simple Euler integration with configurable gravity acceleration.
                Info.MoveInfo.Velocity += FVector(0.0f, 0.0f, MoveData.Gravity) * ScaledDelta;
                Info.MoveInfo.Location += Info.MoveInfo.Velocity * ScaledDelta;
                break;
            }
            case EBulletMoveType::Attached:
            {
                if (Info.InitParams.Owner)
                {
                    Info.MoveInfo.Location = Info.InitParams.Owner->GetActorLocation();
                }
                break;
            }
            case EBulletMoveType::FixedDuration:
            {
                if (Info.MoveInfo.FixedDuration > 0.0f)
                {
                    // Lerp from start to target over a fixed duration (independent of speed).
                    Info.MoveInfo.FixedElapsed += ScaledDelta;
                    const float Alpha = FMath::Clamp(Info.MoveInfo.FixedElapsed / Info.MoveInfo.FixedDuration, 0.0f, 1.0f);
                    Info.MoveInfo.Location = FMath::Lerp(Info.MoveInfo.FixedStartLocation, Info.MoveInfo.FixedTargetLocation, Alpha);
                    if (!Info.MoveInfo.Velocity.IsNearlyZero())
                    {
                        Info.MoveInfo.Rotation = Info.MoveInfo.Velocity.ToOrientationRotator();
                    }
                }
                else
                {
                    Info.MoveInfo.Location += Info.MoveInfo.Velocity * ScaledDelta;
                }
                break;
            }
            case EBulletMoveType::Straight:
            default:
            {
                if (MoveData.bHoming && Info.InitParams.TargetActor)
                {
                    // Basic homing: accelerate toward the desired velocity, clamped by HomingAcceleration.
                    const FVector TargetLocation = Info.InitParams.TargetActor->GetActorLocation();
                    const FVector Desired = (TargetLocation - Info.MoveInfo.Location).GetSafeNormal() * MoveData.Speed;
                    const FVector DeltaVel = Desired - Info.MoveInfo.Velocity;
                    const float MaxAccel = MoveData.HomingAcceleration * ScaledDelta;
                    Info.MoveInfo.Velocity += DeltaVel.GetClampedToMaxSize(MaxAccel);
                }

                Info.MoveInfo.Location += Info.MoveInfo.Velocity * ScaledDelta;
                if (!Info.MoveInfo.Velocity.IsNearlyZero())
                {
                    Info.MoveInfo.Rotation = Info.MoveInfo.Velocity.ToOrientationRotator();
                }
                break;
            }
            }
        }

        if (Info.Actor)
        {
            Info.Actor->SetActorLocationAndRotation(Info.MoveInfo.Location, Info.MoveInfo.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
        }

        if (!Info.bIsSimple && Info.Entity && Info.Entity->GetLogicComponent())
        {
            Info.Entity->GetLogicComponent()->TickLogic(Info, ScaledDelta);
            Info.Entity->GetLogicComponent()->HandlePostMove(Info, ScaledDelta);
        }
    }
}
