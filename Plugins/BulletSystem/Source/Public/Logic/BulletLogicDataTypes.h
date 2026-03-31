#pragma once

// BulletSystem: BulletLogicDataTypes.h
// Logic data assets and controllers.

#include "CoreMinimal.h"
#include "Logic/BulletLogicData.h"
#include "BulletLogicDataTypes.generated.h"

class UCurveVector;
class UGameplayEffect;

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataCreateBullet : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataCreateBullet();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Create")
    FName ChildBulletId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Create", meta = (ClampMin = "0"))
    int32 Count = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Create", meta = (ClampMin = "0"))
    float SpreadAngle = 10.0f;

    // If true, created bullets are cleaned up when the source bullet is destroyed.
    // Disable when the created bullet should continue on its own lifecycle.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Create")
    bool bBindToParentLifetime = true;
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataDestroyBullet : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataDestroyBullet();

    // Destroy is always an explicit immediate request.
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataForce : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataForce();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Force")
    FVector Force = FVector(0.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Force")
    bool bAdditive = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Force")
    bool bForwardSpace = true;
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataFreeze : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataFreeze();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze", meta = (ClampMin = "0"))
    float FreezeTime = 0.0f;
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataRebound : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataRebound();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rebound", meta = (ClampMin = "0"))
    float ReboundFactor = 1.0f;
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataSupport : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataSupport();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Support", meta = (ClampMin = "0"))
    float DamageMultiplier = 1.0f;
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataCurveMovement : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataCurveMovement();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve")
    TObjectPtr<UCurveVector> Curve = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve", meta = (ClampMin = "0"))
    float Duration = 1.0f;
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataShield : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataShield();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield", meta = (ClampMin = "0"))
    float ShieldRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield")
    bool bAttachToOwner = true;
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicData_ApplyGameplayEffect : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicData_ApplyGameplayEffect();

    // GameplayEffect to apply on hit.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GameplayEffect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS", meta = (ClampMin = "0"))
    float EffectLevel = 1.0f;

    // Apply only on server to avoid client-side prediction issues.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
    bool bApplyOnServerOnly = true;

    // Optional dynamic tags granted by the spec (used for hit-react routing, etc).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
    FGameplayTagContainer DynamicGrantedTags;

	// If true, apply the effect once per accepted hit batch, to all actors accepted in that batch.
	// This is useful for "single dispatch" damage logic that wants the full hit batch (multi-target overlaps, etc).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	bool bApplyToAllHitActors = false;

	// If true, dispatch a gameplay event to the target ASC after the GE is applied successfully.
	// This is the most stable way to trigger hit-react abilities while carrying EventData + EffectContext (HitResult, instigator/causer, etc).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
	bool bApplyHitReact = false;
};
