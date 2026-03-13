#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "Model/BulletInfo.h"
#include "BulletActionBase.generated.h"

class UBulletController;

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
