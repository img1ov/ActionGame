#pragma once

// BulletSystem: BulletLogicControllerTypes.h
// Logic data assets and controllers.

#include "CoreMinimal.h"
#include "BulletLogicControllerBlueprintBase.h"
#include "Engine/EngineTypes.h"
#include "Logic/BulletLogicController.h"
#include "Logic/BulletLogicDataTypes.h"
#include "BulletLogicControllerTypes.generated.h"

class AActor;

UCLASS()
class BULLETSYSTEM_API UBulletLogicCreateBulletController : public UBulletLogicControllerBlueprintBase
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicDestroyBulletController : public UBulletLogicControllerBlueprintBase
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicForceController : public UBulletLogicControllerBlueprintBase
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicFreezeController : public UBulletLogicControllerBlueprintBase
{
    GENERATED_BODY()

public:
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicReboundController : public UBulletLogicControllerBlueprintBase
{
    GENERATED_BODY()

public:
    virtual void OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicSupportController : public UBulletLogicControllerBlueprintBase
{
    GENERATED_BODY()

public:
    virtual void OnSupport(FBulletInfo& BulletInfo) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicCurveMovementController : public UBulletLogicControllerBlueprintBase
{
    GENERATED_BODY()

public:
    virtual bool ReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds) override;
};

UCLASS()
class BULLETSYSTEM_API UBulletLogicShieldController : public UBulletLogicControllerBlueprintBase
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
};

UCLASS(Blueprintable)
class BULLETSYSTEM_API UBulletLogicController_ApplyGameplayEffect : public UBulletLogicControllerBlueprintBase
{
    GENERATED_BODY()

public:
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;

protected:
    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|GAS")
    bool ShouldApplyEffect(const FBulletInfo& BulletInfo, const FHitResult& Hit) const;

    // Blueprint hook to apply GAS effects (return true if handled).
    UFUNCTION(BlueprintNativeEvent, DisplayName = "ApplyEffect", Category = "BulletSystem|GAS")
    bool K2_ApplyEffect(UBulletLogicData_ApplyGameplayEffect* ApplyData, AActor* SourceActor, AActor* TargetActor, const FHitResult& Hit, float EffectLevel) const;

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|GAS")
    void OnEffectApplied(const FBulletInfo& BulletInfo, const FHitResult& Hit, bool bSuccess);

private:
    bool ShouldSkipByNetMode(const UBulletLogicData_ApplyGameplayEffect* ApplyData) const;
    bool ApplyEffectToTarget(
        UBulletLogicData_ApplyGameplayEffect* ApplyData,
        const FBulletInfo& BulletInfo,
        AActor* SourceActor,
        AActor* TargetActor,
        const FHitResult& TargetHit) const;
    static FHitResult BuildBestEffortHitForTarget(const FBulletInfo& BulletInfo, const FHitResult& FallbackHit, AActor* TargetActor);

    // Used when bApplyToAllHitActorsAtLastHitTime is enabled to ensure we only apply once per hit batch.
    float LastAppliedBatchHitTime = -BIG_NUMBER;
};
