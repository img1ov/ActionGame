#pragma once

#include "Logging/LogMacros.h"

class UObject;

ACTGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogAct, Log, All);
ACTGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogActExperience, Log, All);
ACTGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogActAbilitySystem, Log, All);
ACTGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogActTeams, Log, All);


ACTGAME_API FString GetClientServerContextString(UObject* ContextObject = nullptr);
