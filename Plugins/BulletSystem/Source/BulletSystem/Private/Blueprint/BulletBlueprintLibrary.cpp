#include "Blueprint/BulletBlueprintLibrary.h"
#include "Controller/BulletWorldSubsystem.h"
#include "Controller/BulletController.h"
#include "Config/BulletConfig.h"
#include "Model/BulletModel.h"
#include "Engine/World.h"

int32 UBulletBlueprintLibrary::CreateBullet(const UObject* WorldContextObject, UBulletConfig* ConfigAsset, FName RowName, const FBulletInitParams& InitParams)
{
    if (!WorldContextObject)
    {
        return INDEX_NONE;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return INDEX_NONE;
    }

    UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
    if (!Subsystem || !Subsystem->GetController())
    {
        return INDEX_NONE;
    }

    int32 BulletId = INDEX_NONE;
    Subsystem->GetController()->CreateBullet(InitParams, RowName, BulletId, ConfigAsset);
    return BulletId;
}

bool UBulletBlueprintLibrary::DestroyBullet(const UObject* WorldContextObject, int32 BulletId, EBulletDestroyReason Reason, bool bSpawnChildren)
{
    if (!WorldContextObject)
    {
        return false;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return false;
    }

    UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
    if (!Subsystem || !Subsystem->GetController())
    {
        return false;
    }

    Subsystem->GetController()->RequestDestroyBullet(BulletId, Reason, bSpawnChildren);
    return true;
}

bool UBulletBlueprintLibrary::IsBulletValid(const UObject* WorldContextObject, int32 BulletId)
{
    if (!WorldContextObject)
    {
        return false;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return false;
    }

    UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
    if (!Subsystem || !Subsystem->GetController())
    {
        return false;
    }

    UBulletModel* Model = Subsystem->GetController()->GetModel();
    return Model && Model->GetBullet(BulletId) != nullptr;
}

bool UBulletBlueprintLibrary::SetBulletCollisionEnabled(const UObject* WorldContextObject, int32 BulletId, bool bEnabled, bool bClearOverlaps, bool bResetHitActors)
{
    if (!WorldContextObject)
    {
        return false;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return false;
    }

    UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
    if (!Subsystem || !Subsystem->GetController())
    {
        return false;
    }

    return Subsystem->GetController()->SetCollisionEnabled(BulletId, bEnabled, bClearOverlaps, bResetHitActors);
}

bool UBulletBlueprintLibrary::ResetBulletHitActors(const UObject* WorldContextObject, int32 BulletId)
{
    if (!WorldContextObject)
    {
        return false;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return false;
    }

    UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
    if (!Subsystem || !Subsystem->GetController())
    {
        return false;
    }

    return Subsystem->GetController()->ResetHitActors(BulletId);
}

int32 UBulletBlueprintLibrary::ApplyDamageToOverlaps(const UObject* WorldContextObject, int32 BulletId, bool bResetHitActorsBefore, bool bApplyCollisionResponse)
{
    if (!WorldContextObject)
    {
        return 0;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return 0;
    }

    UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>();
    if (!Subsystem || !Subsystem->GetController())
    {
        return 0;
    }

    return Subsystem->GetController()->ApplyDamageToOverlaps(BulletId, bResetHitActorsBefore, bApplyCollisionResponse);
}
