#pragma once

// BulletSystem: BulletActions.h
// Queued lifecycle action pipeline.

#include "CoreMinimal.h"
#include "Action/BulletActionBase.h"
#include "BulletActions.generated.h"

UCLASS()
class BULLETGAME_API UBulletActionInitBullet : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionInitHit : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionInitMove : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionInitCollision : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionInitRender : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionTimeScale : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionAfterInit : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionChild : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionSummonBullet : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionSummonEntity : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionDelayDestroyBullet : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
    virtual void Tick(UBulletController* InController, FBulletInfo& BulletInfo, float DeltaSeconds) override;
    virtual bool IsPersistent() const override { return true; }
};

UCLASS()
class BULLETGAME_API UBulletActionSceneInteract : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

UCLASS()
class BULLETGAME_API UBulletActionUpdateEffect : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
    virtual void Tick(UBulletController* InController, FBulletInfo& BulletInfo, float DeltaSeconds) override;
    virtual bool IsPersistent() const override { return true; }
};

UCLASS()
class BULLETGAME_API UBulletActionUpdateLiveTime : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
    virtual void Tick(UBulletController* InController, FBulletInfo& BulletInfo, float DeltaSeconds) override;
    virtual bool IsPersistent() const override { return true; }
};

UCLASS()
class BULLETGAME_API UBulletActionUpdateAttackerFrozen : public UBulletActionBase
{
    GENERATED_BODY()

public:
    virtual void Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo) override;
};

