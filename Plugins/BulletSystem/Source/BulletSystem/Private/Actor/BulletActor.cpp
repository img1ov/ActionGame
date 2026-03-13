#include "Actor/BulletActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

ABulletActor::ABulletActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void ABulletActor::InitializeRender(const FBulletDataRender& RenderData)
{
    if (RenderData.NiagaraSystem.ToSoftObjectPath().IsValid())
    {
        if (!NiagaraComponent)
        {
            NiagaraComponent = NewObject<UNiagaraComponent>(this, TEXT("NiagaraComponent"));
            NiagaraComponent->SetupAttachment(RootComponent);
            NiagaraComponent->RegisterComponent();
        }

        UNiagaraSystem* NiagaraAsset = RenderData.NiagaraSystem.Get();
        if (!NiagaraAsset)
        {
            FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
            NiagaraAsset = Cast<UNiagaraSystem>(Streamable.LoadSynchronous(RenderData.NiagaraSystem.ToSoftObjectPath()));
        }

        NiagaraComponent->SetAsset(NiagaraAsset);
        if (RenderData.bAutoActivate)
        {
            NiagaraComponent->Activate(true);
        }
    }

    if (RenderData.StaticMesh.ToSoftObjectPath().IsValid())
    {
        if (!MeshComponent)
        {
            MeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("MeshComponent"));
            MeshComponent->SetupAttachment(RootComponent);
            MeshComponent->RegisterComponent();
        }

        UStaticMesh* MeshAsset = RenderData.StaticMesh.Get();
        if (!MeshAsset)
        {
            FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
            MeshAsset = Cast<UStaticMesh>(Streamable.LoadSynchronous(RenderData.StaticMesh.ToSoftObjectPath()));
        }

        MeshComponent->SetStaticMesh(MeshAsset);
        MeshComponent->SetRelativeScale3D(RenderData.MeshScale);
        MeshComponent->SetVisibility(true, true);
    }
}

void ABulletActor::ResetActor()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->Deactivate();
    }

    if (MeshComponent)
    {
        MeshComponent->SetVisibility(false, true);
    }

    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

UNiagaraComponent* ABulletActor::GetNiagaraComponent() const
{
    return NiagaraComponent;
}

UStaticMeshComponent* ABulletActor::GetMeshComponent() const
{
    return MeshComponent;
}
