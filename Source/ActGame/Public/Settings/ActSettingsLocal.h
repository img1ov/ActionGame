// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ActSettingsLocal.generated.h"

/**
 * UActSettingsLocal
 */
UCLASS()
class ACTGAME_API UActSettingsLocal : public UGameUserSettings
{
	GENERATED_BODY()

public:

	UActSettingsLocal();

	static UActSettingsLocal* Get();

	void OnExperienceLoaded();
};
