#include "Pool/BulletPools.h"
#include "Entity/BulletEntity.h"
#include "Actor/BulletActor.h"
#include "Engine/World.h"

UBulletEntity* UBulletPool::AcquireEntity(UObject* Outer)
{
    if (InactiveEntities.Num() > 0)
    {
        UBulletEntity* Entity = InactiveEntities.Pop();
        if (Entity)
        {
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

    if (InactiveActors.Num() > 0)
    {
        ABulletActor* Actor = InactiveActors.Pop();
        if (Actor)
        {
            Actor->SetActorHiddenInGame(false);
            Actor->SetActorEnableCollision(false);
            return Actor;
        }
    }

    TSubclassOf<ABulletActor> SpawnClass = RequestedClass ? RequestedClass : DefaultActorClass;
    if (!SpawnClass)
    {
        SpawnClass = ABulletActor::StaticClass();
    }

    return WorldPtr->SpawnActor<ABulletActor>(SpawnClass);
}

void UBulletActorPool::ReleaseActor(ABulletActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Actor->ResetActor();
    InactiveActors.Add(Actor);
}

void UBulletActorPool::Clear()
{
    InactiveActors.Empty();
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
