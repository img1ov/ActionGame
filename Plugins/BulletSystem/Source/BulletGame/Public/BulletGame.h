// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// BulletSystem: BulletGame.h
// Module definitions and glue code.

#include "Modules/ModuleManager.h"

class FBulletGameModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

