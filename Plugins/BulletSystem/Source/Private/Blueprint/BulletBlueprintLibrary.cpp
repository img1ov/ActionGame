// BulletSystem: BulletBlueprintLibrary.cpp
// Blueprint-facing helpers.
#include "Blueprint/BulletBlueprintLibrary.h"
#include "Controller/BulletWorldSubsystem.h"
#include "Controller/BulletController.h"
#include "Model/BulletModel.h"
#include "Engine/World.h"

int32 UBulletBlueprintLibrary::SpawnBullet(const UObject* WorldContextObject, UBulletConfig* ConfigAsset, FName BulletID, const FBulletInitParams& InitParams)
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
    Subsystem->GetController()->SpawnBullet(InitParams, BulletID, BulletId, ConfigAsset);
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

int32 UBulletBlueprintLibrary::ProcessManualHits(const UObject* WorldContextObject, int32 BulletId, bool bResetHitActorsBefore, bool bApplyCollisionResponse)
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

    return Subsystem->GetController()->ProcessManualHits(BulletId, bResetHitActorsBefore, bApplyCollisionResponse);
}

const FBulletPayload& UBulletBlueprintLibrary::SetPayloadSetByCallerMagnitudeByName(FBulletPayload& Payload, FName DataName, float Magnitude)
{
    if (DataName.IsNone())
    {
        return Payload;
    }

    Payload.SetByCallerNameMagnitudes.Add(DataName, Magnitude);
    
    return Payload;
}

const FBulletPayload& UBulletBlueprintLibrary::SetPayloadSetByCallerMagnitudeByTag(FBulletPayload& Payload, FGameplayTag DataTag, float Magnitude)
{
    if (!DataTag.IsValid())
    {
        return Payload;
    }

    Payload.SetByCallerTagMagnitudes.Add(DataTag, Magnitude);
    
    return Payload;
}

void UBulletBlueprintLibrary::SetInitParamsSetByCallerMagnitudeByName(FBulletInitParams& InitParams, FName DataName, float Magnitude)
{
    (void)SetPayloadSetByCallerMagnitudeByName(InitParams.Payload, DataName, Magnitude);
}

void UBulletBlueprintLibrary::SetInitParamsSetByCallerMagnitudeByTag(FBulletInitParams& InitParams, FGameplayTag DataTag, float Magnitude)
{
    (void)SetPayloadSetByCallerMagnitudeByTag(InitParams.Payload, DataTag, Magnitude);
}

void UBulletBlueprintLibrary::ClearPayload(FBulletPayload& Payload)
{
    Payload.SetByCallerNameMagnitudes.Reset();
    Payload.SetByCallerTagMagnitudes.Reset();
}

void UBulletBlueprintLibrary::ClearInitParamsPayload(FBulletInitParams& InitParams)
{
    ClearPayload(InitParams.Payload);
}

bool UBulletBlueprintLibrary::GetPayloadSetByCallerMagnitudeByName(const FBulletInfo& BulletInfo, FName DataName, float& OutMagnitude)
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

bool UBulletBlueprintLibrary::GetPayloadSetByCallerMagnitudeByTag(const FBulletInfo& BulletInfo, FGameplayTag DataTag, float& OutMagnitude)
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

bool UBulletBlueprintLibrary::GetPayloadSetByCallerMagnitudeByNameFromPayload(const FBulletPayload& Payload, FName DataName, float& OutMagnitude)
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

bool UBulletBlueprintLibrary::GetPayloadSetByCallerMagnitudeByTagFromPayload(const FBulletPayload& Payload, FGameplayTag DataTag, float& OutMagnitude)
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
