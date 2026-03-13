#include "System/BulletCollisionSystem.h"
#include "Controller/BulletController.h"
#include "Model/BulletModel.h"
#include "Model/BulletInfo.h"
#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"
#include "Actor/BulletActor.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "DrawDebugHelpers.h"

void UBulletCollisionSystem::OnTick(float DeltaSeconds)
{
    if (!Controller)
    {
        return;
    }

    UWorld* World = Controller->GetWorld();
    UBulletModel* Model = Controller->GetModel();
    if (!World || !Model)
    {
        return;
    }

    const bool bDebugDraw = Controller->IsDebugDrawEnabled();

    for (auto& Pair : Model->GetMutableBulletMap())
    {
        FBulletInfo& Info = Pair.Value;
        if (!Info.bIsInit || Info.bNeedDestroy)
        {
            continue;
        }

        Info.CollisionInfo.bHitThisFrame = false;

        const FVector Start = Info.MoveInfo.LastLocation;
        const FVector End = Info.MoveInfo.Location;
        if (Start.Equals(End, KINDA_SMALL_NUMBER))
        {
            continue;
        }

        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BulletCollision), false);
        if (Info.Actor)
        {
            QueryParams.AddIgnoredActor(Info.Actor);
        }
        if (Info.InitParams.Owner)
        {
            QueryParams.AddIgnoredActor(Info.InitParams.Owner);
        }

        TArray<FHitResult> Hits;
        bool bHit = false;

        const ECollisionChannel Channel = Info.Config.Base.CollisionChannel;

        switch (Info.Config.Base.Shape)
        {
        case EBulletShapeType::Sphere:
        {
            const float Radius = Info.Config.Base.SphereRadius;
            bHit = World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Radius), QueryParams);
            if (bDebugDraw)
            {
                DrawDebugSphere(World, End, Radius, 12, bHit ? FColor::Red : FColor::Green, false, 0.1f);
            }
            break;
        }
        case EBulletShapeType::Box:
        {
            const FVector Extent = Info.Config.Base.BoxExtent;
            bHit = World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeBox(Extent), QueryParams);
            if (bDebugDraw)
            {
                DrawDebugBox(World, End, Extent, bHit ? FColor::Red : FColor::Green, false, 0.1f);
            }
            break;
        }
        case EBulletShapeType::Capsule:
        {
            const float Radius = Info.Config.Base.CapsuleRadius;
            const float HalfHeight = Info.Config.Base.CapsuleHalfHeight;
            bHit = World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams);
            if (bDebugDraw)
            {
                DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, bHit ? FColor::Red : FColor::Green, false, 0.1f);
            }
            break;
        }
        case EBulletShapeType::Ray:
        default:
            bHit = World->LineTraceMultiByChannel(Hits, Start, End, Channel, QueryParams);
            if (bDebugDraw)
            {
                DrawDebugLine(World, Start, End, bHit ? FColor::Red : FColor::Green, false, 0.1f);
            }
            break;
        }

        if (!bHit)
        {
            continue;
        }

        for (const FHitResult& Hit : Hits)
        {
            AActor* HitActor = Hit.GetActor();
            if (!HitActor)
            {
                continue;
            }

            if (Info.CollisionInfo.HitActors.Contains(HitActor))
            {
                continue;
            }

            Info.CollisionInfo.HitActors.Add(HitActor);
            Info.CollisionInfo.HitCount++;
            Info.CollisionInfo.LastHitTime = World->GetTimeSeconds();
            Info.CollisionInfo.bHitThisFrame = true;

            if (Info.Entity && Info.Entity->GetLogicComponent())
            {
                Info.Entity->GetLogicComponent()->HandleOnHit(Info, Hit);

                if (HitActor->IsA<ABulletActor>())
                {
                    const int32 OtherBulletId = Controller->FindBulletIdByActor(HitActor);
                    if (OtherBulletId != INDEX_NONE)
                    {
                        Info.Entity->GetLogicComponent()->HandleOnHitBullet(Info, OtherBulletId);
                    }
                }
            }

            if (Info.Config.Children.bSpawnOnHit)
            {
                Controller->RequestSummonChildren(Info);
            }

            const bool bHitLimitReached = Info.CollisionInfo.HitCount >= Info.Config.Base.MaxHitCount;
            const bool bDestroyOnHit = Info.Config.Base.bDestroyOnHit || bHitLimitReached;

            if (Info.Config.Base.Shape == EBulletShapeType::Ray)
            {
                Info.RayInfo.TraceStart = Start;
                Info.RayInfo.TraceEnd = End;
            }

            if (Info.Config.Base.CollisionResponse == EBulletCollisionResponse::Destroy && bDestroyOnHit)
            {
                Controller->RequestDestroyBullet(Info.BulletId, EBulletDestroyReason::Hit, true);
                break;
            }

            if (Info.Config.Base.CollisionResponse == EBulletCollisionResponse::Bounce)
            {
                const FVector Reflected = FMath::GetReflectionVector(Info.MoveInfo.Velocity, Hit.ImpactNormal);
                Info.MoveInfo.Velocity = Reflected;
                if (Info.Entity && Info.Entity->GetLogicComponent())
                {
                    Info.Entity->GetLogicComponent()->HandleOnRebound(Info, Hit);
                }
            }
        }
    }
}
