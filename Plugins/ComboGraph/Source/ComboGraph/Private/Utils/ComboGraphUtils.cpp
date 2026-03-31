#include "Utils/ComboGraphUtils.h"

#include "Engine/World.h"
#include "Misc/EngineVersionComparison.h"

bool FComboGraphUtils::IsAnimationPreviewActor(const AActor* OwnerActor)
{
	// We test owner name instead of class to not have to rely on Persona module (which is used only in Editor, and is using UnrealEd. Prevents packaging)
	return GetNameSafe(OwnerActor).StartsWith(TEXT("AnimationEditorPreviewActor"));
}

const UComboGraphProjectSettings* FComboGraphUtils::GetPluginProjectSettings()
{
	const UComboGraphProjectSettings* Settings = GetDefault<UComboGraphProjectSettings>();
	check(Settings);
	return Settings;
}

FString FComboGraphUtils::GetWorldLogPrefix(UWorld* World)
{
	FString Prefix;
	if (World)
	{
		if (World->WorldType == EWorldType::PIE)
		{
#if UE_VERSION_OLDER_THAN(5, 5, 0)
			const int32 PlayInEditorID = GPlayInEditorID;
#else
			const int32 PlayInEditorID = UE::GetPlayInEditorID();
#endif
			switch(World->GetNetMode())
			{
			case NM_Client:
				// GPlayInEditorID 0 is always the server, so 1 will be first client.
				// You want to keep this logic in sync with GeneratePIEViewportWindowTitle and UpdatePlayInEditorWorldDebugString
				Prefix = FString::Printf(TEXT("Client %d: "), PlayInEditorID);
				break;
			case NM_DedicatedServer:
			case NM_ListenServer:
				Prefix = FString::Printf(TEXT("Server: "));
				break;
			case NM_Standalone:
				Prefix = FString::Printf(TEXT("Standalone: "));
			default:
				break;
			}
		}
	}

	return Prefix;
}

