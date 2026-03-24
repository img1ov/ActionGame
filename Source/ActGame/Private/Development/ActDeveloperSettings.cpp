// Copyright Epic Games, Inc. All Rights Reserved.

#include "Development/ActDeveloperSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActDeveloperSettings)

#define LOCTEXT_NAMESPACE "ActCheats"

namespace Act::CVars
{
	static constexpr const TCHAR* ShouldAlwaysPlayForceFeedback = TEXT("ActPC.ShouldAlwaysPlayForceFeedback");
}

static void EnsureActDeveloperSettingsCVarsExist()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	if (ConsoleManager.FindConsoleVariable(Act::CVars::ShouldAlwaysPlayForceFeedback) == nullptr)
	{
		// UDeveloperSettingsBackedByCVars will fatal if a ConsoleVariable-backed property references a missing CVar.
		// Register it here to avoid relying on static init order across translation units.
		ConsoleManager.RegisterConsoleVariable(
			Act::CVars::ShouldAlwaysPlayForceFeedback,
			0,
			TEXT("Should force feedback effects be played, even if the last input device was not a gamepad?"),
			ECVF_Default);
	}
}

UActDeveloperSettings::UActDeveloperSettings()
{
}

FName UActDeveloperSettings::GetCategoryName() const
{
	return FApp::GetProjectName();
}

void UActDeveloperSettings::PostInitProperties()
{
	EnsureActDeveloperSettingsCVarsExist();
	Super::PostInitProperties();

#if WITH_EDITOR
	ApplySettings();
#endif
}

#if WITH_EDITOR
void UActDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplySettings();
}

void UActDeveloperSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);

	ApplySettings();
}

void UActDeveloperSettings::ApplySettings()
{
}

void UActDeveloperSettings::OnPlayInEditorStarted() const
{
	// Show a notification toast to remind the user that there's an experience override set
	if (ExperienceOverride.IsValid())
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("ExperienceOverrideActive", "Developer Settings Override\nExperience {0}"),
			FText::FromName(ExperienceOverride.PrimaryAssetName)
		));
		Info.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}
#endif

#undef LOCTEXT_NAMESPACE

