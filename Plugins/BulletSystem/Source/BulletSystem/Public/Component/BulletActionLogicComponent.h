#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "BulletActionLogicComponent.generated.h"

class UBulletLogicController;
class UBulletLogicData;
class UBulletController;
class UBulletEntity;
struct FBulletInfo;
struct FHitResult;

UCLASS()
class BULLETSYSTEM_API UBulletActionLogicComponent : public UObject
{
    GENERATED_BODY()

public:
    // Builds and dispatches logic controllers defined by execution data.
    void Initialize(UBulletController* InController, UBulletEntity* InEntity, const FBulletDataExecution& ExecutionData);
    void Reset();

    void HandleOnBegin(FBulletInfo& BulletInfo);
    void HandleOnHit(FBulletInfo& BulletInfo, const FHitResult& Hit);
    void HandleOnDestroy(FBulletInfo& BulletInfo, EBulletDestroyReason Reason);
    void HandleOnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit);
    void HandleOnSupport(FBulletInfo& BulletInfo);
    void HandleOnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId);
    void TickLogic(FBulletInfo& BulletInfo, float DeltaSeconds);

    bool HandleReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds);

private:
    void AddController(UBulletLogicController* Controller, EBulletLogicTrigger Trigger);

    UPROPERTY()
    TObjectPtr<UBulletController> Controller;

    UPROPERTY()
    TObjectPtr<UBulletEntity> Entity;

    UPROPERTY()
    TArray<TObjectPtr<UBulletLogicController>> OnBeginControllers;

    UPROPERTY()
    TArray<TObjectPtr<UBulletLogicController>> OnHitControllers;

    UPROPERTY()
    TArray<TObjectPtr<UBulletLogicController>> OnDestroyControllers;

    UPROPERTY()
    TArray<TObjectPtr<UBulletLogicController>> OnReboundControllers;

    UPROPERTY()
    TArray<TObjectPtr<UBulletLogicController>> OnSupportControllers;

    UPROPERTY()
    TArray<TObjectPtr<UBulletLogicController>> ReplaceMoveControllers;

    UPROPERTY()
    TArray<TObjectPtr<UBulletLogicController>> OnHitBulletControllers;

    UPROPERTY()
    TArray<TObjectPtr<UBulletLogicController>> TickControllers;
};
