#include "System/BulletCollisionSystem.h"
#include "Controller/BulletController.h"
#include "Model/BulletModel.h"
#include "Model/BulletInfo.h"
#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"
#include "Actor/BulletActor.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
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

        if (!Info.CollisionInfo.bCollisionEnabled)
        {
            Info.CollisionInfo.OverlapActors.Reset();
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
        const bool bManualHit = Info.Config.Base.HitTrigger == EBulletHitTrigger::Manual;
        const bool bOverlapMode = Info.Config.Base.CollisionMode == EBulletCollisionMode::Overlap;

        if (bOverlapMode)
        {
            TSet<TWeakObjectPtr<AActor>> NewOverlaps;
            const FVector Center = Info.MoveInfo.Location;
            const FQuat Rotation = Info.MoveInfo.Rotation.Quaternion();
            bool bAnyOverlap = false;

            switch (Info.Config.Base.Shape)
            {
            case EBulletShapeType::Sphere:
            {
                const float Radius = Info.Config.Base.SphereRadius;
                TArray<FOverlapResult> Overlaps;
                bAnyOverlap = World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Radius), QueryParams);
                for (const FOverlapResult& Overlap : Overlaps)
                {
                    if (AActor* HitActor = Overlap.GetActor())
                    {
                        NewOverlaps.Add(HitActor);
                    }
                }
                if (bDebugDraw)
                {
                    DrawDebugSphere(World, Center, Radius, 12, bAnyOverlap ? FColor::Red : FColor::Green, false, 0.1f);
                }
                break;
            }
            case EBulletShapeType::Box:
            {
                const FVector Extent = Info.Config.Base.BoxExtent;
                TArray<FOverlapResult> Overlaps;
                bAnyOverlap = World->OverlapMultiByChannel(Overlaps, Center, Rotation, Channel, FCollisionShape::MakeBox(Extent), QueryParams);
                for (const FOverlapResult& Overlap : Overlaps)
                {
                    if (AActor* HitActor = Overlap.GetActor())
                    {
                        NewOverlaps.Add(HitActor);
                    }
                }
                if (bDebugDraw)
                {
                    DrawDebugBox(World, Center, Extent, Rotation, bAnyOverlap ? FColor::Red : FColor::Green, false, 0.1f);
                }
                break;
            }
            case EBulletShapeType::Capsule:
            {
                const float Radius = Info.Config.Base.CapsuleRadius;
                const float HalfHeight = Info.Config.Base.CapsuleHalfHeight;
                TArray<FOverlapResult> Overlaps;
                bAnyOverlap = World->OverlapMultiByChannel(Overlaps, Center, Rotation, Channel, FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams);
                for (const FOverlapResult& Overlap : Overlaps)
                {
                    if (AActor* HitActor = Overlap.GetActor())
                    {
                        NewOverlaps.Add(HitActor);
                    }
                }
                if (bDebugDraw)
                {
                    DrawDebugCapsule(World, Center, HalfHeight, Radius, Rotation, bAnyOverlap ? FColor::Red : FColor::Green, false, 0.1f);
                }
                break;
            }
            case EBulletShapeType::Ray:
            default:
            {
                const FVector Start = Info.MoveInfo.LastLocation;
                const FVector End = Info.MoveInfo.Location;
                bAnyOverlap = World->LineTraceMultiByChannel(Hits, Start, End, Channel, QueryParams);
                for (const FHitResult& Hit : Hits)
                {
                    if (AActor* HitActor = Hit.GetActor())
                    {
                        NewOverlaps.Add(HitActor);
                    }
                }
                if (bDebugDraw)
                {
                    DrawDebugLine(World, Start, End, bAnyOverlap ? FColor::Red : FColor::Green, false, 0.1f);
                }
                break;
            }
            }

            Info.CollisionInfo.OverlapActors = MoveTemp(NewOverlaps);

            if (!bManualHit)
            {
                for (const TWeakObjectPtr<AActor>& ActorPtr : Info.CollisionInfo.OverlapActors)
                {
                    AActor* HitActor = ActorPtr.Get();
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

                    const FVector HitLocation = HitActor->GetActorLocation();
                    const FVector HitNormal = (HitLocation - Center).GetSafeNormal();
                    FHitResult Hit(HitActor, nullptr, HitLocation, HitNormal);
                    Hit.Location = HitLocation;
                    Hit.ImpactPoint = HitLocation;
                    Hit.TraceStart = Center;
                    Hit.TraceEnd = Center;
                    Hit.ImpactNormal = HitNormal;

                    const bool bDestroyed = Controller->HandleHitResult(Info, HitActor, Hit, true);
                    if (bDestroyed)
                    {
                        break;
                    }
                }
            }
            continue;
        }

        const FVector Start = Info.MoveInfo.LastLocation;
        const FVector End = Info.MoveInfo.Location;
        if (Start.Equals(End, KINDA_SMALL_NUMBER))
        {
            Info.CollisionInfo.OverlapActors.Reset();
            continue;
        }

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
            Info.CollisionInfo.OverlapActors.Reset();
            continue;
        }

        if (bManualHit)
        {
            TSet<TWeakObjectPtr<AActor>> NewOverlaps;
            for (const FHitResult& Hit : Hits)
            {
                if (AActor* HitActor = Hit.GetActor())
                {
                    NewOverlaps.Add(HitActor);
                }
            }
            Info.CollisionInfo.OverlapActors = MoveTemp(NewOverlaps);
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

            const bool bDestroyed = Controller->HandleHitResult(Info, HitActor, Hit, true);
            if (bDestroyed)
            {
                break;
            }
        }
    }
}
