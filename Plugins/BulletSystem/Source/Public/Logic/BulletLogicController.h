#pragma once

// BulletSystem: BulletLogicControllers.h
// Logic data assets and controllers.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "BulletLogicController.generated.h"

class UBulletController;
class UBulletEntity;
class UBulletLogicData;
struct FBulletInfo;
struct FHitResult;

UCLASS(Abstract, NotBlueprintable)
class BULLETSYSTEM_API UBulletLogicController : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UBulletController* InController, UBulletEntity* InEntity, UBulletLogicData* InData);

    UFUNCTION(BlueprintPure, Category = "BulletSystem|Logic")
    UBulletController* GetBulletController() const { return Controller; }

    UFUNCTION(BlueprintPure, Category = "BulletSystem|Logic")
    UBulletEntity* GetBulletEntity() const { return Entity; }

    UFUNCTION(BlueprintPure, Category = "BulletSystem|Logic")
    UBulletLogicData* GetLogicData() const { return Data; }

    virtual void OnBegin(FBulletInfo& BulletInfo) {}
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) {}
    virtual void OnDestroy(FBulletInfo& BulletInfo) {}
    virtual void OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit) {}
    virtual void OnSupport(FBulletInfo& BulletInfo) {}
    virtual void OnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId) {}
    virtual void Tick(FBulletInfo& BulletInfo, float DeltaSeconds) {}
    virtual bool ReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds) { return false; }
    // Extension hooks for advanced movement/collision without changing core systems.
    virtual void OnPreMove(FBulletInfo& BulletInfo, float DeltaSeconds) {}
    virtual void OnPostMove(FBulletInfo& BulletInfo, float DeltaSeconds) {}
    virtual void OnPreCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits) {}
    virtual void OnPostCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits) {}
    virtual bool FilterHit(FBulletInfo& BulletInfo, const FHitResult& Hit) { return true; }

protected:
    UPROPERTY()
    TObjectPtr<UBulletController> Controller;

    UPROPERTY()
    TObjectPtr<UBulletEntity> Entity;

    UPROPERTY()
    TObjectPtr<UBulletLogicData> Data;
};
