#pragma once

// BulletSystem: BulletLogicData.h
// Logic data assets and controllers.

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BulletSystemTypes.h"
#include "BulletLogicData.generated.h"

class UBulletLogicController;

UCLASS(Abstract, BlueprintType)
class BULLETGAME_API UBulletLogicData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    EBulletLogicTrigger Trigger = EBulletLogicTrigger::OnBegin;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    TSubclassOf<UBulletLogicController> ControllerClass;
};


