#pragma once

// BulletSystem: BulletActionLogicComponent.h
// Components used by bullet entities/actors.

#include "CoreMinimal.h"
#include "Component/BulletEntityComponent.h"
#include "BulletSystemTypes.h"
#include "BulletActionLogicComponent.generated.h"

class UBulletLogicController;
class UBulletLogicData;
class UBulletController;
class UBulletEntity;
struct FBulletInfo;
struct FHitResult;

UCLASS()
class BULLETGAME_API UBulletActionLogicComponent : public UBulletEntityComponent
{
    GENERATED_BODY()

public:
    // Builds and dispatches logic controllers defined by execution data.
    void Initialize(UBulletController* InController, UBulletEntity* InEntity, const FBulletDataExecution& ExecutionData);
    virtual void Reset() override;

    void HandleOnBegin(FBulletInfo& BulletInfo);
    void HandleOnHit(FBulletInfo& BulletInfo, const FHitResult& Hit);
    void HandleOnDestroy(FBulletInfo& BulletInfo, EBulletDestroyReason Reason);
    void HandleOnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit);
    void HandleOnSupport(FBulletInfo& BulletInfo);
    void HandleOnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId);
    void TickLogic(FBulletInfo& BulletInfo, float DeltaSeconds);

    bool HandleReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds);
    void HandlePreMove(FBulletInfo& BulletInfo, float DeltaSeconds);
    void HandlePostMove(FBulletInfo& BulletInfo, float DeltaSeconds);
    void HandlePreCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits);
    void HandlePostCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits);
    bool FilterHit(FBulletInfo& BulletInfo, const FHitResult& Hit);

private:
    void AddController(UBulletLogicController* Controller, EBulletLogicTrigger Trigger);

    UPROPERTY()
    TObjectPtr<UBulletController> Controller;

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

    UPROPERTY()
    TArray<TObjectPtr<UBulletLogicController>> AllControllers;
};

