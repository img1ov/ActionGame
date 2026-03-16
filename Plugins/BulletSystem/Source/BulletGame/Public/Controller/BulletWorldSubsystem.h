#pragma once

// BulletSystem: BulletWorldSubsystem.h
// Runtime controller layer and orchestration.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "BulletWorldSubsystem.generated.h"

class UBulletController;

UCLASS()
class BULLETGAME_API UBulletWorldSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaSeconds) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return true; }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

    UBulletController* GetController() const;

private:
    UPROPERTY()
    TObjectPtr<UBulletController> Controller;
};

