// Copyright

#include "AbilitySystem/Bullets/AN_SpawnBulletSimProxy.h"

#include "Blueprint/BulletSystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UAN_SpawnBulletSimProxy::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || BulletID.IsNone())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (!World || World->IsNetMode(NM_DedicatedServer))
	{
		return;
	}

	// Only fire on simulated proxies:
	// - AutonomousProxy: owning client runs GAS and should spawn the predicted bullet via GA flow.
	// - Authority: server should spawn the authoritative bullet via GA flow.
	if (OwnerActor->GetLocalRole() != ROLE_SimulatedProxy)
	{
		return;
	}

	FTransform SpawnTransform = MeshComp->GetComponentTransform();
	if (!SpawnSocketName.IsNone() && MeshComp->DoesSocketExist(SpawnSocketName))
	{
		SpawnTransform = MeshComp->GetSocketTransform(SpawnSocketName, RTS_World);
	}

	FBulletInitParams InitParams;
	InitParams.Owner = OwnerActor;
	InitParams.SpawnTransform = SpawnTransform;
	InitParams.SpawnOffset = SpawnOffset;
	InitParams.CollisionEnabledOverride = bEnableCollision ? 1 : 0;

	// The pawn/character implements IBulletSystemInterface in this project, so passing OwnerActor is enough.
	// BulletSystemBlueprintLibrary will route to the BulletSystemComponent on PlayerState when present.
	(void)UBulletSystemBlueprintLibrary::SpawnBullet(OwnerActor, BulletID, InitParams);
}
