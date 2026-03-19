// BulletSystem: BulletLogicControllers.cpp
// Logic data assets and controllers.
#include "Logic/BulletLogicController.h"

// Bind controller to runtime context (controller/entity/data).
void UBulletLogicController::Initialize(UBulletController* InController, UBulletEntity* InEntity, UBulletLogicData* InData)
{
    Controller = InController;
    Entity = InEntity;
    Data = InData;
}
