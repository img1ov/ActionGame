// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// BulletSystem: BulletSystem.h
// Module definitions and glue code.

#include "Modules/ModuleManager.h"

class FBulletSystemModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

