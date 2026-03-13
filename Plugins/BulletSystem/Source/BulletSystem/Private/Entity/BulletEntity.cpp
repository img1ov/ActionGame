#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"

void UBulletEntity::Initialize(int32 InBulletId)
{
    BulletId = InBulletId;
    if (!LogicComponent)
    {
        LogicComponent = NewObject<UBulletActionLogicComponent>(this);
    }
    LogicComponent->Reset();
    Actor = nullptr;
}

void UBulletEntity::Reset()
{
    BulletId = INDEX_NONE;
    if (LogicComponent)
    {
        LogicComponent->Reset();
    }
    Actor = nullptr;
}

int32 UBulletEntity::GetBulletId() const
{
    return BulletId;
}

UBulletActionLogicComponent* UBulletEntity::GetLogicComponent() const
{
    return LogicComponent;
}

void UBulletEntity::SetActor(ABulletActor* InActor)
{
    Actor = InActor;
}

ABulletActor* UBulletEntity::GetActor() const
{
    return Actor;
}
