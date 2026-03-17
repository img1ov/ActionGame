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

    // Lifecycle triggers.
    void HandleOnBegin(FBulletInfo& BulletInfo);
    void HandleOnHit(FBulletInfo& BulletInfo, const FHitResult& Hit);
    void HandleOnDestroy(FBulletInfo& BulletInfo, EBulletDestroyReason Reason);
    void HandleOnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit);
    void HandleOnSupport(FBulletInfo& BulletInfo);
    void HandleOnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId);
    void TickLogic(FBulletInfo& BulletInfo, float DeltaSeconds);

    // Movement/collision hooks. ReplaceMove can short-circuit the default movement system.
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
    // Controllers fired once when the bullet is initialized (spawn/begin play).
    TArray<TObjectPtr<UBulletLogicController>> OnBeginControllers;

    UPROPERTY()
    // Controllers fired per hit (after hit filtering and interval gating).
    TArray<TObjectPtr<UBulletLogicController>> OnHitControllers;

    UPROPERTY()
    // Controllers fired when bullet is destroyed (destroy reason provided).
    TArray<TObjectPtr<UBulletLogicController>> OnDestroyControllers;

    UPROPERTY()
    // Controllers fired when the bullet bounces/rebounds.
    TArray<TObjectPtr<UBulletLogicController>> OnReboundControllers;

    UPROPERTY()
    // Controllers fired when the bullet supports/attaches (custom meaning per project).
    TArray<TObjectPtr<UBulletLogicController>> OnSupportControllers;

    UPROPERTY()
    // Controllers that can replace movement. If any returns true, the movement system will not apply default motion.
    TArray<TObjectPtr<UBulletLogicController>> ReplaceMoveControllers;

    UPROPERTY()
    // Controllers fired when this bullet hits another bullet (bullet-bullet interactions).
    TArray<TObjectPtr<UBulletLogicController>> OnHitBulletControllers;

    UPROPERTY()
    // Controllers ticked every frame after movement (per-bullet logic update).
    TArray<TObjectPtr<UBulletLogicController>> TickControllers;

    UPROPERTY()
    // Flat list of all controllers for uniform pre/post move/collision hooks and filtering.
    TArray<TObjectPtr<UBulletLogicController>> AllControllers;
};

