#pragma once

// BulletSystem: BulletLogicData.h
// Logic data assets and controllers.

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BulletSystemTypes.h"
#include "Runtime/Engine/Classes/Engine/DataAsset.h"
#include "BulletLogicData.generated.h"

class UBulletLogicController;

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletLogicData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    EBulletLogicTrigger Trigger = EBulletLogicTrigger::OnBegin;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    TSubclassOf<UBulletLogicController> ControllerClass;
};
