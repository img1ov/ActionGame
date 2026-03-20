#include "Pool/BulletPools.h"
#include "Entity/BulletEntity.h"
#include "Actor/BulletActor.h"
#include "Engine/World.h"

UBulletEntity* UBulletPool::AcquireEntity(UObject* Outer)
{
    if (InactiveEntities.Num() > 0)
    {
        if (UBulletEntity* Entity = InactiveEntities.Pop())
        {
            // Re-parent to the current outer so the entity participates in the correct GC cluster/lifetime.
            Entity->Rename(nullptr, Outer);
            return Entity;
        }
    }

    return NewObject<UBulletEntity>(Outer);
}

void UBulletPool::ReleaseEntity(UBulletEntity* Entity)
{
    if (!Entity)
    {
        return;
    }

    Entity->Reset();
    InactiveEntities.Add(Entity);
}

void UBulletPool::Clear()
{
    InactiveEntities.Reset();
}

void UBulletActorPool::Initialize(UWorld* InWorld, TSubclassOf<ABulletActor> InDefaultClass)
{
    World = InWorld;
    DefaultActorClass = InDefaultClass;
}

ABulletActor* UBulletActorPool::AcquireActor(TSubclassOf<ABulletActor> RequestedClass)
{
    UWorld* WorldPtr = World.Get();
    if (!WorldPtr)
    {
        return nullptr;
    }

    UClass* SpawnClass = RequestedClass.Get();
    if (!SpawnClass)
    {
        SpawnClass = DefaultActorClass.Get();
    }
    if (!SpawnClass)
    {
        SpawnClass = ABulletActor::StaticClass();
    }

    // Pool actors per class to avoid reusing mismatched render setups (mesh/niagara/components can differ by class).
    if (FBulletActorPoolBucket* Bucket = InactiveActorsByClass.Find(SpawnClass))
    {
        while (Bucket->Actors.Num() > 0)
        {
            ABulletActor* Actor = Bucket->Actors.Pop(EAllowShrinking::No);
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != WorldPtr)
            {
                continue;
            }

            Actor->SetActorHiddenInGame(false);
            // Render actors are controlled by the bullet system; collision is handled by the collision system.
            Actor->SetActorEnableCollision(false);
            return Actor;
        }
    }

    return WorldPtr->SpawnActor<ABulletActor>(SpawnClass);
}

void UBulletActorPool::ReleaseActor(ABulletActor* Actor)
{
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
    {
        return;
    }

    Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Actor->ResetActor();
    InactiveActorsByClass.FindOrAdd(Actor->GetClass()).Actors.Add(Actor);
}

void UBulletActorPool::Clear()
{
    for (auto& Pair : InactiveActorsByClass)
    {
        for (ABulletActor* Actor : Pair.Value.Actors)
        {
            if (IsValid(Actor) && !Actor->IsActorBeingDestroyed())
            {
                Actor->Destroy();
            }
        }
    }
    InactiveActorsByClass.Empty();
}

FBulletTraceElement UBulletTraceElementPool::Acquire()
{
    if (Pool.Num() > 0)
    {
        FBulletTraceElement Element = Pool.Pop();
        return Element;
    }

    return FBulletTraceElement();
}

void UBulletTraceElementPool::Release(const FBulletTraceElement& Element)
{
    Pool.Add(Element);
}

void UBulletTraceElementPool::Clear()
{
    Pool.Reset();
}
