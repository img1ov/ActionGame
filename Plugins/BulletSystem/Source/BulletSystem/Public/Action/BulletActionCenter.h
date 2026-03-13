#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "BulletActionCenter.generated.h"

class UBulletActionBase;
class UBulletController;

USTRUCT()
struct FBulletActionPool
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<TObjectPtr<UBulletActionBase>> Actions;
};

UCLASS()
class BULLETSYSTEM_API UBulletActionCenter : public UObject
{
    GENERATED_BODY()

public:
    // Factory + pool for action instances.
    void Initialize(UBulletController* InController);

    UBulletActionBase* AcquireAction(EBulletActionType ActionType);
    void ReleaseAction(UBulletActionBase* Action);

    FBulletActionInfo AcquireActionInfo();
    void ReleaseActionInfo(const FBulletActionInfo& ActionInfo);

private:
    UClass* GetActionClass(EBulletActionType ActionType) const;

    UPROPERTY()
    TObjectPtr<UBulletController> Controller;

    UPROPERTY()
    TMap<UClass*, FBulletActionPool> ActionPools;

    UPROPERTY()
    TArray<FBulletActionInfo> ActionInfoPool;
};
