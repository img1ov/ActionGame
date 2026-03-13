// Fill out your copyright notice in the Description page of Project Settings.


#include "Settings/ActSettingsLocal.h"

UActSettingsLocal::UActSettingsLocal()
{
}

UActSettingsLocal* UActSettingsLocal::Get()
{
	return GEngine ? CastChecked<UActSettingsLocal>(GEngine->GetGameUserSettings()) : nullptr;
}

void UActSettingsLocal::OnExperienceLoaded()
{
}
