#pragma once

// BulletSystem: BulletWorldSubsystem.h
// Runtime controller layer and orchestration.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "BulletWorldSubsystem.generated.h"

class UBulletController;
class AActor;

UCLASS()
class BULLETGAME_API UBulletWorldSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // Creates the UBulletController for this world and initializes all bullet runtime systems.
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    // Shuts down the controller and releases pooled resources.
    virtual void Deinitialize() override;
    // Called when the world begins play. Used to clear pools for PIE re-entry.
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    // Main world tick hook. Delegates to UBulletController::OnTick / OnAfterTick.
    virtual void Tick(float DeltaSeconds) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return true; }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

    // Returns the controller for this world (may be null during teardown).
    UBulletController* GetController() const;

private:
    // Owned controller instance. Lives for the lifetime of this world subsystem.
    UPROPERTY()
    TObjectPtr<UBulletController> Controller;

    void HandleWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);
    void HandleEndPlayMap();

    FDelegateHandle WorldCleanupHandle;
    FDelegateHandle EndPlayMapHandle;
};
