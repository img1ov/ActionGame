#pragma once

// BulletSystem: BulletActionBase.h
// Queued lifecycle action pipeline.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "BulletActionBase.generated.h"

class UBulletController;
struct FBulletInfo;

UCLASS(Abstract)
class BULLETSYSTEM_API UBulletActionBase : public UObject
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) {}
    virtual void Tick(UBulletController* InController, FBulletInfo& BulletInfo, float DeltaSeconds) { (void)InController; (void)BulletInfo; (void)DeltaSeconds; }
    virtual void AfterTick(UBulletController* InController, FBulletInfo& BulletInfo, float DeltaSeconds) { (void)InController; (void)BulletInfo; (void)DeltaSeconds; }
    virtual bool IsPersistent() const { return false; }
};

