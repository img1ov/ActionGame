#include "System/BulletMoveSystem.h"
#include "Controller/BulletController.h"
#include "Model/BulletModel.h"
#include "Model/BulletInfo.h"
#include "Actor/BulletActor.h"
#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"
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

    for (auto& Pair : Model->GetMutableBulletMap())
    {
        FBulletInfo& Info = Pair.Value;
        if (!Info.bIsInit || Info.bNeedDestroy)
        {
            continue;
        }

        if (Info.bFrozen || (Info.FrozenUntilTime > 0.0f && Info.FrozenUntilTime > WorldTime))
        {
            continue;
        }

        const float ScaledDelta = DeltaSeconds * Info.TimeScale;
        Info.MoveInfo.LastLocation = Info.MoveInfo.Location;

        bool bSkipDefaultMove = false;
        if (Info.Entity && Info.Entity->GetLogicComponent())
        {
            bSkipDefaultMove = Info.Entity->GetLogicComponent()->HandleReplaceMove(Info, ScaledDelta);
        }

        const FBulletDataMove& MoveData = Info.Config.Move;

        if (bSkipDefaultMove && !Info.MoveInfo.CustomMoveCurve)
        {
            if (Info.Actor)
            {
                Info.Actor->SetActorLocationAndRotation(Info.MoveInfo.Location, Info.MoveInfo.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
            }

            if (Info.Entity && Info.Entity->GetLogicComponent())
            {
                Info.Entity->GetLogicComponent()->TickLogic(Info, ScaledDelta);
            }
            continue;
        }

        if (Info.MoveInfo.bAttachToOwner && Info.InitParams.Owner)
        {
            Info.MoveInfo.Location = Info.InitParams.Owner->GetActorLocation();
        }
        else if (Info.MoveInfo.CustomMoveCurve && Info.MoveInfo.CustomMoveDuration > 0.0f)
        {
            Info.MoveInfo.CustomMoveElapsed += ScaledDelta;
            const float Alpha = FMath::Clamp(Info.MoveInfo.CustomMoveElapsed / Info.MoveInfo.CustomMoveDuration, 0.0f, 1.0f);
            const FVector CurveOffset = Info.MoveInfo.CustomMoveCurve->GetVectorValue(Alpha);
            Info.MoveInfo.Location = Info.MoveInfo.Location + CurveOffset * ScaledDelta;
        }
        else
        {
            switch (MoveData.MoveType)
            {
            case EBulletMoveType::Orbit:
            {
                if (Info.MoveInfo.OrbitCenter.IsNearlyZero() && Info.InitParams.Owner)
                {
                    Info.MoveInfo.OrbitCenter = Info.InitParams.Owner->GetActorLocation();
                }
                Info.MoveInfo.OrbitAngle += MoveData.OrbitAngularSpeed * ScaledDelta;
                const float Radius = MoveData.OrbitRadius;
                const float AngleRad = FMath::DegreesToRadians(Info.MoveInfo.OrbitAngle);
                Info.MoveInfo.Location = Info.MoveInfo.OrbitCenter + FVector(FMath::Cos(AngleRad) * Radius, FMath::Sin(AngleRad) * Radius, 0.0f);
                break;
            }
            case EBulletMoveType::FollowTarget:
            {
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
            case EBulletMoveType::Straight:
            default:
            {
                if (MoveData.bHoming && Info.InitParams.TargetActor)
                {
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

        if (Info.Entity && Info.Entity->GetLogicComponent())
        {
            Info.Entity->GetLogicComponent()->TickLogic(Info, ScaledDelta);
        }
    }
}
