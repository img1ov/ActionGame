#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "BulletLogicControllers.generated.h"

class UBulletController;
class UBulletEntity;
class UBulletLogicData;
struct FBulletInfo;
struct FHitResult;

UCLASS(Abstract)
class BULLETSYSTEM_API UBulletLogicController : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UBulletController* InController, UBulletEntity* InEntity, UBulletLogicData* InData);

    virtual void OnBegin(FBulletInfo& BulletInfo) {}
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) {}
    virtual void OnDestroy(FBulletInfo& BulletInfo, EBulletDestroyReason Reason) {}
    virtual void OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit) {}
    virtual void OnSupport(FBulletInfo& BulletInfo) {}
    virtual void OnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId) {}
    virtual void Tick(FBulletInfo& BulletInfo, float DeltaSeconds) {}
    virtual bool ReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds) { return false; }

protected:
    UPROPERTY()
    TObjectPtr<UBulletController> Controller;

    UPROPERTY()
    TObjectPtr<UBulletEntity> Entity;

    UPROPERTY()
    TObjectPtr<UBulletLogicData> Data;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicCreateBulletController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicDestroyBulletController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicForceController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicFreezeController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicReboundController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicSupportController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnSupport(FBulletInfo& BulletInfo) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicCurveMovementController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual bool ReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicShieldController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
};
