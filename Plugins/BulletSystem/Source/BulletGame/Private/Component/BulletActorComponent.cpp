// BulletSystem: BulletActorComponent.cpp
// Components used by bullet entities/actors.
#include "Component/BulletActorComponent.h"
#include "Actor/BulletActor.h"

void UBulletActorComponent::Reset()
{
    Super::Reset();
    Actor = nullptr;
}

void UBulletActorComponent::SetActor(ABulletActor* InActor)
{
    Actor = InActor;
}

ABulletActor* UBulletActorComponent::GetActor() const
{
    return Actor;
}

