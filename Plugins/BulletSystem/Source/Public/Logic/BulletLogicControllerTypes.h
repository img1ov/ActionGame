#pragma once

// BulletSystem: BulletLogicControllerTypes.h
// Logic data assets and controllers.

#include "CoreMinimal.h"
#include "BulletLogicControllerBlueprintBase.h"
#include "Engine/EngineTypes.h"
#include "Logic/BulletLogicController.h"
#include "Logic/BulletLogicDataTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "BulletSystemTypes.h"
#include "BulletLogicControllerTypes.generated.h"

class AActor;

USTRUCT()
struct BULLETSYSTEM_API FHitReactImpulseTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FHitReactImpulse HitReactImpulse;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FString ToString() const override { return TEXT("FHitReactImpulseTargetData"); }

	// NetSerialize is used via TStructOpsTypeTraits (WithNetSerializer = true).
	// Note: FGameplayAbilityTargetData doesn't declare NetSerialize as a virtual in all UE versions, so do not use override.
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;

		bool bTagSuccess = true;
		HitReactImpulse.Tag.NetSerialize(Ar, Map, bTagSuccess);
		bOutSuccess &= bTagSuccess;

		Ar << HitReactImpulse.Direction;
		Ar << HitReactImpulse.Strength;

		return bOutSuccess;
	}
};

template<>
struct TStructOpsTypeTraits<FHitReactImpulseTargetData> : public TStructOpsTypeTraitsBase2<FHitReactImpulseTargetData>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

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

    // Used when bApplyToAllHitActors is enabled to ensure we only apply once per hit batch (even if multiple actors
    // trigger OnHit for the same batch).
    uint32 LastAppliedBatchId = 0;
};
