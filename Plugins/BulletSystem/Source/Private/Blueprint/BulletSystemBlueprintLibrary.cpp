// BulletSystem: BulletBlueprintLibrary.cpp
// Blueprint-facing helpers.
#include "Blueprint/BulletSystemBlueprintLibrary.h"
#include "BulletLogChannels.h"
#include "BulletSystemComponent.h"
#include "Config/BulletConfig.h"
#include "Controller/BulletWorldSubsystem.h"
#include "Controller/BulletController.h"
#include "BulletSystemInterface.h"
#include "Model/BulletModel.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

int32 UBulletSystemBlueprintLibrary::SpawnBullet(AActor* SourceActor, FName BulletId, const FBulletInitParams& InitParams)
{
    if (!SourceActor)
    {
        return INDEX_NONE;
    }

    UBulletSystemComponent* BulletComponent = nullptr;
    if (SourceActor->GetClass()->ImplementsInterface(UBulletSystemInterface::StaticClass()))
    {
        BulletComponent = IBulletSystemInterface::Execute_GetBulletSystemComponent(SourceActor);
    }

    if (!BulletComponent)
    {
        BulletComponent = SourceActor->FindComponentByClass<UBulletSystemComponent>();
    }

    if (!BulletComponent)
    {
        UE_LOG(LogBullet, Warning, TEXT("BulletSystemBlueprintLibrary: SpawnBullet failed, BulletSystemComponent missing. SourceActor=%s BulletId=%s"),
            *GetNameSafe(SourceActor),
            *BulletId.ToString());
        return INDEX_NONE;
    }

    return BulletComponent->SpawnBullet(BulletId, InitParams);
}

int32 UBulletSystemBlueprintLibrary::GetInstanceIdByKey(AActor* SourceActor, FName InstanceKey)
{
    if (!SourceActor || InstanceKey.IsNone())
    {
        return INDEX_NONE;
    }

    UBulletSystemComponent* BulletComponent = nullptr;
    if (SourceActor->GetClass()->ImplementsInterface(UBulletSystemInterface::StaticClass()))
    {
        BulletComponent = IBulletSystemInterface::Execute_GetBulletSystemComponent(SourceActor);
    }

    if (!BulletComponent)
    {
        BulletComponent = SourceActor->FindComponentByClass<UBulletSystemComponent>();
    }

    return BulletComponent ? BulletComponent->GetInstanceIdByKey(InstanceKey) : INDEX_NONE;
}

bool UBulletSystemBlueprintLibrary::DestroyBullet(const UObject* WorldContextObject, int32 InstanceId)
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

    Subsystem->GetController()->RequestDestroyBullet(InstanceId);
    return true;
}

bool UBulletSystemBlueprintLibrary::IsBulletValid(const UObject* WorldContextObject, int32 InstanceId)
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
    if (!Model)
    {
        return false;
    }

    const FBulletInfo* Info = Model->GetBullet(InstanceId);
    return Info != nullptr && !Info->bNeedDestroy;
}

bool UBulletSystemBlueprintLibrary::SetBulletCollisionEnabled(const UObject* WorldContextObject, int32 InstanceId, bool bEnabled, bool bClearOverlaps, bool bResetHitActors)
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

    return Subsystem->GetController()->SetCollisionEnabled(InstanceId, bEnabled, bClearOverlaps, bResetHitActors);
}

bool UBulletSystemBlueprintLibrary::ResetBulletHitActors(const UObject* WorldContextObject, int32 InstanceId)
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

    return Subsystem->GetController()->ResetHitActors(InstanceId);
}

int32 UBulletSystemBlueprintLibrary::ProcessManualHits(const UObject* WorldContextObject, int32 InstanceId, bool bResetHitActorsBefore, bool bApplyCollisionResponse)
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

    return Subsystem->GetController()->ProcessManualHits(InstanceId, bResetHitActorsBefore, bApplyCollisionResponse);
}

TArray<AActor*> UBulletSystemBlueprintLibrary::GetBulletHitActors(const FBulletInfo& BulletInfo)
{
    TArray<AActor*> OutActors;
    OutActors.Reserve(BulletInfo.CollisionInfo.HitActors.Num());
    for (const TPair<TWeakObjectPtr<AActor>, float>& Pair : BulletInfo.CollisionInfo.HitActors)
    {
        if (AActor* HitActor = Pair.Key.Get())
        {
            OutActors.Add(HitActor);
        }
    }
    return OutActors;
}

TArray<AActor*> UBulletSystemBlueprintLibrary::GetBulletHitActorsAtLastHitTime(const FBulletInfo& BulletInfo, float TimeTolerance)
{
    TArray<AActor*> OutActors;
    const float HitTime = BulletInfo.CollisionInfo.LastHitTime;
    if (HitTime <= -BIG_NUMBER)
    {
        return OutActors;
    }

    OutActors.Reserve(BulletInfo.CollisionInfo.HitActors.Num());
    for (const TPair<TWeakObjectPtr<AActor>, float>& Pair : BulletInfo.CollisionInfo.HitActors)
    {
        if (!FMath::IsNearlyEqual(Pair.Value, HitTime, TimeTolerance))
        {
            continue;
        }

        if (AActor* HitActor = Pair.Key.Get())
        {
            OutActors.Add(HitActor);
        }
    }
    return OutActors;
}

void UBulletSystemBlueprintLibrary::ClearBulletSystemRuntime(const UObject* WorldContextObject, bool bRebuildRuntimeTable)
{
    if (!WorldContextObject)
    {
        return;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return;
    }

    if (UBulletWorldSubsystem* Subsystem = World->GetSubsystem<UBulletWorldSubsystem>())
    {
        if (UBulletController* Controller = Subsystem->GetController())
        {
            Controller->Shutdown();
        }
    }

    if (bRebuildRuntimeTable)
    {
        // Rebuild config runtime tables used by this world so PIE config edits take effect immediately.
        // (UBulletConfig caches a runtime lookup table that is normally rebuilt lazily.)
        TSet<TWeakObjectPtr<UBulletConfig>> UniqueConfigs;
        for (TObjectIterator<UBulletSystemComponent> It; It; ++It)
        {
            UBulletSystemComponent* Comp = *It;
            if (!Comp || Comp->HasAnyFlags(RF_ClassDefaultObject) || Comp->GetWorld() != World)
            {
                continue;
            }

            if (UBulletConfig* Config = Comp->GetBulletConfig())
            {
                UniqueConfigs.Add(Config);
            }
        }

        for (const TWeakObjectPtr<UBulletConfig>& ConfigPtr : UniqueConfigs)
        {
            if (const UBulletConfig* Config = ConfigPtr.Get())
            {
                Config->RebuildRuntimeTable();
            }
        }
    }
}

const FBulletPayload& UBulletSystemBlueprintLibrary::SetPayloadSetByCallerMagnitudeByName(FBulletPayload& Payload, FName DataName, float Magnitude)
{
    if (DataName.IsNone())
    {
        return Payload;
    }

    Payload.SetByCallerNameMagnitudes.Add(DataName, Magnitude);
    
    return Payload;
}

const FBulletPayload& UBulletSystemBlueprintLibrary::SetPayloadSetByCallerMagnitudeByTag(FBulletPayload& Payload, FGameplayTag DataTag, float Magnitude)
{
    if (!DataTag.IsValid())
    {
        return Payload;
    }

    Payload.SetByCallerTagMagnitudes.Add(DataTag, Magnitude);
    
    return Payload;
}

void UBulletSystemBlueprintLibrary::SetInitParamsSetByCallerMagnitudeByName(FBulletInitParams& InitParams, FName DataName, float Magnitude)
{
    (void)SetPayloadSetByCallerMagnitudeByName(InitParams.Payload, DataName, Magnitude);
}

void UBulletSystemBlueprintLibrary::SetInitParamsSetByCallerMagnitudeByTag(FBulletInitParams& InitParams, FGameplayTag DataTag, float Magnitude)
{
    (void)SetPayloadSetByCallerMagnitudeByTag(InitParams.Payload, DataTag, Magnitude);
}

void UBulletSystemBlueprintLibrary::ClearPayload(FBulletPayload& Payload)
{
    Payload.SetByCallerNameMagnitudes.Reset();
    Payload.SetByCallerTagMagnitudes.Reset();
}

void UBulletSystemBlueprintLibrary::ClearInitParamsPayload(FBulletInitParams& InitParams)
{
    ClearPayload(InitParams.Payload);
}

FBulletInitParams UBulletSystemBlueprintLibrary::SetBulletPayload(FBulletInitParams InitParams, const FBulletPayload& Payload)
{
    InitParams.Payload = Payload;
    return InitParams;
}

bool UBulletSystemBlueprintLibrary::GetPayloadSetByCallerMagnitudeByName(const FBulletInfo& BulletInfo, FName DataName, float& OutMagnitude)
{
    const float* Found = DataName.IsNone() ? nullptr : BulletInfo.InitParams.Payload.SetByCallerNameMagnitudes.Find(DataName);
    if (!Found)
    {
        OutMagnitude = 0.0f;
        return false;
    }

    OutMagnitude = *Found;
    return true;
}

bool UBulletSystemBlueprintLibrary::GetPayloadSetByCallerMagnitudeByTag(const FBulletInfo& BulletInfo, FGameplayTag DataTag, float& OutMagnitude)
{
    const float* Found = DataTag.IsValid() ? BulletInfo.InitParams.Payload.SetByCallerTagMagnitudes.Find(DataTag) : nullptr;
    if (!Found)
    {
        OutMagnitude = 0.0f;
        return false;
    }

    OutMagnitude = *Found;
    return true;
}

bool UBulletSystemBlueprintLibrary::GetPayloadSetByCallerMagnitudeByNameFromPayload(const FBulletPayload& Payload, FName DataName, float& OutMagnitude)
{
    const float* Found = DataName.IsNone() ? nullptr : Payload.SetByCallerNameMagnitudes.Find(DataName);
    if (!Found)
    {
        OutMagnitude = 0.0f;
        return false;
    }

    OutMagnitude = *Found;
    return true;
}

bool UBulletSystemBlueprintLibrary::GetPayloadSetByCallerMagnitudeByTagFromPayload(const FBulletPayload& Payload, FGameplayTag DataTag, float& OutMagnitude)
{
    const float* Found = DataTag.IsValid() ? Payload.SetByCallerTagMagnitudes.Find(DataTag) : nullptr;
    if (!Found)
    {
        OutMagnitude = 0.0f;
        return false;
    }

    OutMagnitude = *Found;
    return true;
}
