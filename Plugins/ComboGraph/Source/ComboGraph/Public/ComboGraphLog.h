#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"

COMBOGRAPH_API DECLARE_LOG_CATEGORY_EXTERN(LogComboGraph, Display, All);

namespace UE::ComboGraph::Log
{
	/** Log debug helper to return the appropriate world prefix in logs (Server, Client, etc.) */
	static FString GetWorldLogPrefix(const UWorld* InWorld)
	{
		FString WorldName;

		if (InWorld == nullptr)
		{
			WorldName = NSLOCTEXT("ComboGraphLog", "PlayWorldBeingCreated", "(World Being Created)").ToString();
		}
		else
		{
			const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(InWorld);

			switch (InWorld->GetNetMode())
			{
			case NM_Standalone:
				WorldName = NSLOCTEXT("ComboGraphLog", "PlayWorldIsStandalone", "Standalone").ToString();
				break;

			case NM_ListenServer:
			case NM_DedicatedServer:
				WorldName = NSLOCTEXT("ComboGraphLog", "PlayWorldIsServer", "Server  ").ToString();
				break;

			case NM_Client:
				// 0 is always the server, use PIEInstance so it matches the in-editor UI
				WorldName = FText::Format(NSLOCTEXT("ComboGraphLog", "PlayWorldIsClient", "Client {0}"), FText::AsNumber(WorldContext ? WorldContext->PIEInstance : 0)).ToString();
				break;

			default:
				break;
			}

			if ((WorldContext != nullptr) && !WorldContext->CustomDescription.IsEmpty())
			{
				WorldName += TEXT(" ") + WorldContext->CustomDescription;
			}
		}

		return WorldName;
	}

	/** Helper method with screen logger to associate a given verbosity to an on-screen color */
	static FColor GetOnScreenVerbosityColor(const ELogVerbosity::Type Verbosity)
	{
		return
			Verbosity == ELogVerbosity::Fatal || Verbosity == ELogVerbosity::Error ? FColor::Red :
			Verbosity == ELogVerbosity::Warning ? FColor::Yellow :
			Verbosity == ELogVerbosity::Display || Verbosity == ELogVerbosity::Log ? FColor::Cyan :
			Verbosity == ELogVerbosity::Verbose || Verbosity == ELogVerbosity::VeryVerbose ? FColor::Orange :
			FColor::Cyan;
	}

	/** Draws log to the viewport screen */
	static void AddOnScreenDebugMessage(const ELogVerbosity::Type Verbosity, const FString& Message)
	{
		if (GEngine)
		{
			const FColor Color = GetOnScreenVerbosityColor(Verbosity);
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, Color, Message);
		}
	}
};

#define CG_RUNTIME_LOG(Verbosity, Format, ...) \
{ \
	UE_LOG(LogComboGraph, Verbosity, Format, ##__VA_ARGS__); \
}

#define CG_RUNTIME_WLOG(Verbosity, Format, ...) \
{ \
	UE_LOG(LogComboGraph, Verbosity, TEXT("[%s] %s - %s"), *UE::ComboGraph::Log::GetWorldLogPrefix(GetWorld()), *FString(__FUNCTION__), *FString::Printf(Format, ##__VA_ARGS__)); \
}

#define CG_RUNTIME_WLOG_WITHOUT_NS(Verbosity, Format, ...) \
{ \
	UE_LOG(LogComboGraph, Verbosity, TEXT("[%s] %s"), *UE::ComboGraph::Log::GetWorldLogPrefix(GetWorld()), *FString::Printf(Format, ##__VA_ARGS__)); \
}

#define CG_RUNTIME_NSLOG(Verbosity, Format, ...) \
{ \
	UE_LOG(LogComboGraph, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(Format, ##__VA_ARGS__)); \
}

#define CG_RUNTIME_CLOG(Condition, Verbosity, Format, ...) \
{ \
	UE_CLOG(Condition, LogComboGraph, Verbosity, Format, ##__VA_ARGS__); \
}

#define CG_RUNTIME_SLOG(Verbosity, Format, ...) \
{ \
	UE::ComboGraph::Log::AddOnScreenDebugMessage(ELogVerbosity::Verbosity, FString::Printf(Format, ##__VA_ARGS__)); \
	UE_LOG(LogComboGraph, Verbosity, Format, ##__VA_ARGS__); \
}

#define CG_RUNTIME_LOG_SCREEN(Verbosity, bPrintScreen, Format, ...) \
{ \
	if (bPrintScreen) \
	{ \
		UE::ComboGraph::Log::AddOnScreenDebugMessage(ELogVerbosity::Verbosity, FString::Printf(Format, ##__VA_ARGS__)); \
	} \
	UE_LOG(LogComboGraph, Verbosity, Format, ##__VA_ARGS__); \
}

FORCEINLINE FString GetBoolText(const bool bCondition)
{
	return bCondition ? TEXT("true") : TEXT("false");
}
