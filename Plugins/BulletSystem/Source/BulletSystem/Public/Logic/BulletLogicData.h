#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BulletSystemTypes.h"
#include "BulletLogicData.generated.h"

class UBulletLogicController;
class UCurveVector;

UCLASS(Abstract, BlueprintType)
class BULLETSYSTEM_API UBulletLogicData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    EBulletLogicTrigger Trigger = EBulletLogicTrigger::OnBegin;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    TSubclassOf<UBulletLogicController> ControllerClass;
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataCreateBullet : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataCreateBullet();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Create")
    FName ChildRowName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Create", meta = (ClampMin = "0"))
    int32 Count = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Create", meta = (ClampMin = "0"))
    float SpreadAngle = 10.0f;
};

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicDataDestroyBullet : public UBulletLogicData
{
    GENERATED_BODY()

public:
    UBulletLogicDataDestroyBullet();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destroy")
    EBulletDestroyReason Reason = EBulletDestroyReason::Logic;
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
