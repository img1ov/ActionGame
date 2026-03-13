// Fill out your copyright notice in the Description page of Project Settings.


#include "System/ActAssetManager.h"

#include "ActLogChannels.h"

const FName FActBundles::Equipped("Equipped");

UActAssetManager::UActAssetManager()
{
}

UActAssetManager& UActAssetManager::Get()
{
	check(GEngine);

	if (UActAssetManager* Singleton = Cast<UActAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOG(LogAct, Fatal, TEXT("Invalid AssetManagerClassName in DefaultEngine.ini.  It must be set to LyraAssetManager!"));

	// Fatal error above prevents this from being called.
	return *NewObject<UActAssetManager>();
}
