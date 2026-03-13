#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BulletSystemTypes.h"
#include "BulletActor.generated.h"

class UNiagaraComponent;
class UStaticMeshComponent;

UCLASS()
class BULLETSYSTEM_API ABulletActor : public AActor
{
    GENERATED_BODY()

public:
    ABulletActor();

    void InitializeRender(const FBulletDataRender& RenderData);
    void ResetActor();

    UNiagaraComponent* GetNiagaraComponent() const;
    UStaticMeshComponent* GetMeshComponent() const;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> NiagaraComponent;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> MeshComponent;
};
