// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/AssetManager.h"
#include "ActAssetManager.generated.h"

struct FActBundles
{
	static const FName Equipped;
};

/**
 * UActAssetManager
 *
 *	Game implementation of the asset manager that overrides functionality and stores game-specific types.
 *	It is expected that most games will want to override AssetManager as it provides a good place for game-specific loading logic.
 *	This class is used by setting 'AssetManagerClassName' in DefaultEngine.ini.
 */
UCLASS()
class ACTGAME_API UActAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	UActAssetManager();

	// Returns the AssetManager singleton object.
	static UActAssetManager& Get();
};
