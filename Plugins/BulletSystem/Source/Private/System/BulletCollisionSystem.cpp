#include "System/BulletCollisionSystem.h"
#include "Controller/BulletController.h"
#include "Model/BulletModel.h"
#include "Model/BulletInfo.h"
#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"
#include "Actor/BulletActor.h"
#include "Component/BulletBudgetComponent.h"
#include "Pool/BulletPools.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "BulletLogChannel.h"

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

    #if WITH_EDITOR
    const bool bDebugDraw = Controller->IsDebugDrawEnabled();
    const float DebugDrawDuration = 0.0f;
    #endif
    const float WorldTime = World->GetTimeSeconds();

    UBulletTraceElementPool* TracePool = Controller->GetTraceElementPool();
    #if WITH_EDITOR
    constexpr int32 MaxDebugActors = 8;
    #endif

    // Snapshot ids to keep iteration stable if hit logic queues new bullets (avoid TMap iterator invalidation).
    TArray<int32> InstanceIds;
    InstanceIds.Reserve(Model->GetBulletMap().Num());
    for (const auto& Pair : Model->GetBulletMap())
    {
        InstanceIds.Add(Pair.Key);
    }

    for (int32 InstanceId : InstanceIds)
    {
        FBulletInfo* InfoPtr = Model->GetBullet(InstanceId);
        if (!InfoPtr)
        {
            continue;
        }
        FBulletInfo& Info = *InfoPtr;
        if (!Info.bIsInit || Info.bNeedDestroy)
        {
            continue;
        }

        float RawDelta = DeltaSeconds;
        if (Info.Entity && Info.Entity->GetBudgetComponent())
        {
            // Collision can be decimated independently from movement to reduce expensive queries.
            if (!Info.Entity->GetBudgetComponent()->ConsumeCollisionTick(WorldTime, DeltaSeconds, RawDelta))
            {
                continue;
            }
        }

        // TraceElement is a small temporary struct used by collision logic (debug/telemetry).
        // It's pooled to reduce per-bullet allocations in tight loops.
        FBulletTraceElement TraceElement = TracePool ? TracePool->Acquire() : FBulletTraceElement();
        TraceElement.Start = Info.MoveInfo.LastLocation;
        TraceElement.End = Info.MoveInfo.Location;
        auto ReleaseTrace = [&]()
        {
            if (TracePool)
            {
                TracePool->Release(TraceElement);
            }
        };

        Info.CollisionInfo.bHitThisFrame = false;

        // Respect collision activation delay (e.g. muzzle-exit grace period).
        if (Info.Config.Base.CollisionStartDelay > 0.0f && (WorldTime - Info.SpawnWorldTime) < Info.Config.Base.CollisionStartDelay)
        {
            Info.CollisionInfo.OverlapActors.Reset();
            ReleaseTrace();
            continue;
        }

        if (!Info.CollisionInfo.bCollisionEnabled)
        {
            Info.CollisionInfo.OverlapActors.Reset();
            ReleaseTrace();
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
        const float HitInterval = Info.Config.Base.HitInterval;

        // Optional "obstacle" trace: a separate static-only sweep that can destroy the bullet before regular hit logic.
        // This is typically used to keep projectiles from clipping through walls when their normal channel is permissive.
        if (Info.Config.Obstacle.bEnableObstacle)
        {
            const float ObstacleRadius = Info.Config.Obstacle.ObstacleTraceRadius > 0.0f ? Info.Config.Obstacle.ObstacleTraceRadius : Info.Config.Base.SphereRadius;
            FHitResult ObstacleHit;
            if (const bool bObstacleHit = World->SweepSingleByChannel(ObstacleHit, Info.MoveInfo.LastLocation, Info.MoveInfo.Location, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(ObstacleRadius), QueryParams))
            {
                UE_LOG(LogBullet, Verbose, TEXT("Obstacle hit: InstanceId=%d"), Info.InstanceId);
                const bool bDestroyed = Controller->HandleHitResult(Info, ObstacleHit.GetActor(), ObstacleHit, true);
                if (bDestroyed)
                {
                    ReleaseTrace();
                    continue;
                }
            }
        }

        if (bOverlapMode)
        {
            // Overlap mode: collect current overlaps for the configured shape.
            // Auto-hit: may immediately apply hit logic.
            // Manual-hit: only stores overlaps for later processing (ProcessManualHits).
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
                #if WITH_EDITOR
                if (bDebugDraw)
                {
                    DrawDebugSphere(World, Center, Radius, 12, bAnyOverlap ? FColor::Red : FColor::Green, false, DebugDrawDuration);
                }
                #endif
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
                #if WITH_EDITOR
                if (bDebugDraw)
                {
                    DrawDebugBox(World, Center, Extent, Rotation, bAnyOverlap ? FColor::Red : FColor::Green, false, DebugDrawDuration);
                }
                #endif
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
                #if WITH_EDITOR
                if (bDebugDraw)
                {
                    DrawDebugCapsule(World, Center, HalfHeight, Radius, Rotation, bAnyOverlap ? FColor::Red : FColor::Green, false, DebugDrawDuration);
                }
                #endif
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
                #if WITH_EDITOR
                if (bDebugDraw)
                {
                    DrawDebugLine(World, Start, End, bAnyOverlap ? FColor::Red : FColor::Green, false, DebugDrawDuration);
                }
                #endif
                break;
            }
            }

            Info.CollisionInfo.OverlapActors = MoveTemp(NewOverlaps);

            #if WITH_EDITOR
            UE_LOG(LogBullet, VeryVerbose, TEXT("Overlap collect: InstanceId=%d Count=%d"), Info.InstanceId, Info.CollisionInfo.OverlapActors.Num());
            if (Info.CollisionInfo.OverlapActors.Num() > 0)
            {
                int32 Logged = 0;
                for (const TWeakObjectPtr<AActor>& ActorPtr : Info.CollisionInfo.OverlapActors)
                {
                    if (AActor* HitActor = ActorPtr.Get())
                    {
                        UE_LOG(LogBullet, VeryVerbose, TEXT("  OverlapActor[%d]=%s"), Logged, *HitActor->GetName());
                        if (++Logged >= MaxDebugActors)
                        {
                            break;
                        }
                    }
                }
            }
            #endif

            if (!bManualHit)
            {
                // Auto-hit: allow logic to inspect the collision batch before we dispatch per-actor hit callbacks.
                if (!Info.bIsSimple && Info.Entity && Info.Entity->GetLogicComponent())
                {
                    Info.Entity->GetLogicComponent()->HandlePreCollision(Info, Hits);
                }

                for (const TWeakObjectPtr<AActor>& ActorPtr : Info.CollisionInfo.OverlapActors)
                {
                    AActor* HitActor = ActorPtr.Get();
                    if (!HitActor)
                    {
                        continue;
                    }

                    // HitInterval gates repeated hits on the same actor (DoT / multi-hit projectiles).
                    if (const float* LastHit = Info.CollisionInfo.HitActors.Find(HitActor))
                    {
                        // HitInterval <= 0 means "no gating" (hit every time).
                        if (HitInterval > 0.0f && (WorldTime - *LastHit) < HitInterval)
                        {
                            continue;
                        }
                    }

                    Info.CollisionInfo.HitActors.Add(HitActor, WorldTime);
                    Info.CollisionInfo.HitCount++;
                    Info.CollisionInfo.LastHitTime = WorldTime;
                    Info.CollisionInfo.bHitThisFrame = true;

                    const FVector HitLocation = HitActor->GetActorLocation();
                    const FVector HitNormal = (HitLocation - Center).GetSafeNormal();
                    FHitResult Hit(HitActor, nullptr, HitLocation, HitNormal);
                    Hit.Location = HitLocation;
                    Hit.ImpactPoint = HitLocation;
                    Hit.TraceStart = Center;
                    Hit.TraceEnd = Center;
                    Hit.ImpactNormal = HitNormal;

                    if (!Info.bIsSimple && Info.Entity && Info.Entity->GetLogicComponent())
                    {
                        if (!Info.Entity->GetLogicComponent()->FilterHit(Info, Hit))
                        {
                            continue;
                        }
                    }

                    const bool bDestroyed = Controller->HandleHitResult(Info, HitActor, Hit, true);
                    if (bDestroyed)
                    {
                        break;
                    }
                }

                if (!Info.bIsSimple && Info.Entity && Info.Entity->GetLogicComponent())
                {
                    Info.Entity->GetLogicComponent()->HandlePostCollision(Info, Hits);
                }
            }
            ReleaseTrace();
            continue;
        }

        const FVector Start = TraceElement.Start;
        const FVector End = TraceElement.End;
        if (Start.Equals(End, KINDA_SMALL_NUMBER))
        {
            Info.CollisionInfo.OverlapActors.Reset();
            ReleaseTrace();
            continue;
        }

        switch (Info.Config.Base.Shape)
        {
        case EBulletShapeType::Sphere:
        {
            const float Radius = Info.Config.Base.SphereRadius;
            bHit = World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Radius), QueryParams);
            #if WITH_EDITOR
            if (bDebugDraw)
            {
                DrawDebugSphere(World, End, Radius, 12, bHit ? FColor::Red : FColor::Green, false, DebugDrawDuration);
            }
            #endif
            break;
        }
        case EBulletShapeType::Box:
        {
            const FVector Extent = Info.Config.Base.BoxExtent;
            bHit = World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeBox(Extent), QueryParams);
            #if WITH_EDITOR
            if (bDebugDraw)
            {
                DrawDebugBox(World, End, Extent, bHit ? FColor::Red : FColor::Green, false, DebugDrawDuration);
            }
            #endif
            break;
        }
        case EBulletShapeType::Capsule:
        {
            const float Radius = Info.Config.Base.CapsuleRadius;
            const float HalfHeight = Info.Config.Base.CapsuleHalfHeight;
            bHit = World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams);
            #if WITH_EDITOR
            if (bDebugDraw)
            {
                DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, bHit ? FColor::Red : FColor::Green, false, DebugDrawDuration);
            }
            #endif
            break;
        }
        case EBulletShapeType::Ray:
        default:
            bHit = World->LineTraceMultiByChannel(Hits, Start, End, Channel, QueryParams);
            #if WITH_EDITOR
            if (bDebugDraw)
            {
                DrawDebugLine(World, Start, End, bHit ? FColor::Red : FColor::Green, false, DebugDrawDuration);
            }
            #endif
            break;
        }

        if (!bHit)
        {
            Info.CollisionInfo.OverlapActors.Reset();
            ReleaseTrace();
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
            #if WITH_EDITOR
            UE_LOG(LogBullet, VeryVerbose, TEXT("Sweep collect (manual): InstanceId=%d Count=%d"), Info.InstanceId, Info.CollisionInfo.OverlapActors.Num());
            if (Info.CollisionInfo.OverlapActors.Num() > 0)
            {
                int32 Logged = 0;
                for (const TWeakObjectPtr<AActor>& ActorPtr : Info.CollisionInfo.OverlapActors)
                {
                    if (AActor* HitActor = ActorPtr.Get())
                    {
                        UE_LOG(LogBullet, VeryVerbose, TEXT("  OverlapActor[%d]=%s"), Logged, *HitActor->GetName());
                        if (++Logged >= MaxDebugActors)
                        {
                            break;
                        }
                    }
                }
            }
            #endif
            ReleaseTrace();
            continue;
        }

        if (!Info.bIsSimple && Info.Entity && Info.Entity->GetLogicComponent())
        {
            Info.Entity->GetLogicComponent()->HandlePreCollision(Info, Hits);
        }

        #if WITH_EDITOR
        UE_LOG(LogBullet, VeryVerbose, TEXT("Sweep hits: InstanceId=%d Count=%d"), Info.InstanceId, Hits.Num());
        #endif
        for (const FHitResult& Hit : Hits)
        {
            AActor* HitActor = Hit.GetActor();
            if (!HitActor)
            {
                continue;
            }

            #if WITH_EDITOR
            UE_LOG(LogBullet, VeryVerbose, TEXT("  HitActor=%s"), *HitActor->GetName());
            #endif

            // HitInterval gates repeated hits on the same actor (DoT / multi-hit projectiles).
            if (const float* LastHit = Info.CollisionInfo.HitActors.Find(HitActor))
            {
                // HitInterval <= 0 means "no gating" (hit every time).
                if (HitInterval > 0.0f && (WorldTime - *LastHit) < HitInterval)
                {
                    continue;
                }
            }

            Info.CollisionInfo.HitActors.Add(HitActor, WorldTime);
            Info.CollisionInfo.HitCount++;
            Info.CollisionInfo.LastHitTime = WorldTime;
            Info.CollisionInfo.bHitThisFrame = true;

            if (!Info.bIsSimple && Info.Entity && Info.Entity->GetLogicComponent())
            {
                if (!Info.Entity->GetLogicComponent()->FilterHit(Info, Hit))
                {
                    continue;
                }
            }

            const bool bDestroyed = Controller->HandleHitResult(Info, HitActor, Hit, true);
            if (bDestroyed)
            {
                break;
            }
        }

        if (!Info.bIsSimple && Info.Entity && Info.Entity->GetLogicComponent())
        {
            Info.Entity->GetLogicComponent()->HandlePostCollision(Info, Hits);
        }

        ReleaseTrace();
    }
}
