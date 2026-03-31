#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FComboGraphModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void SetupSettings();
	void ShutdownSettings();

	static void OnPostEngineInit();

	static void EnsureGlobalData();
	static bool HandleSettingsSaved();
};
