#pragma once

// BulletSystem: BulletLogicControllerTypes.h
// Logic data assets and controllers.

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Model/BulletInfo.h"
#include "Logic/BulletLogicControllers.h"
#include "Logic/BulletLogicDataTypes.h"
#include "BulletLogicControllerTypes.generated.h"

class AActor;

UCLASS()
class BULLETGAME_API UBulletLogicCreateBulletController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETGAME_API UBulletLogicDestroyBulletController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETGAME_API UBulletLogicForceController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletLogicFreezeController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETGAME_API UBulletLogicReboundController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETGAME_API UBulletLogicSupportController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnSupport(FBulletInfo& BulletInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletLogicCurveMovementController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual bool ReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds) override;
};

UCLASS()
class BULLETGAME_API UBulletLogicShieldController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
};

UCLASS(Blueprintable)
class BULLETGAME_API UBulletLogicApplyGEController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;

protected:
    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|GAS")
    bool ShouldApplyEffect(const FBulletInfo& BulletInfo, const FHitResult& Hit) const;

    // Blueprint hook to apply GAS effects (return true if handled).
    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|GAS")
    bool ApplyEffectBlueprint(UBulletLogicDataApplyGE* ApplyData, AActor* SourceActor, AActor* TargetActor, const FHitResult& Hit, float EffectLevel) const;

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|GAS")
    void OnEffectApplied(const FBulletInfo& BulletInfo, const FHitResult& Hit, bool bSuccess);
};

