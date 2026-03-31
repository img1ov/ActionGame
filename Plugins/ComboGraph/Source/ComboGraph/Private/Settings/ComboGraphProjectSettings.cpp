#include "Settings/ComboGraphProjectSettings.h"

#include "Abilities/Tasks/ComboGraphAbilityTask_NetworkSyncPoint.h"
#include "AnimNotifies/ComboGraphANS_ComboWindow.h"
#include "ComboGraphLog.h"

namespace UE::ComboGraph
{
	static FAutoConsoleCommand ConsoleCommandDumpSettings(
		TEXT("ComboGraph.DumpSyncSettings"),
		TEXT(""),
		FConsoleCommandWithArgsDelegate::CreateStatic(DumpSyncSettings)
	);
	
}

void UE::ComboGraph::DumpSyncSettings(const TArray<FString>& InArgs)
{
	const UComboGraphProjectSettings& Settings = UComboGraphProjectSettings::Get();

	CG_RUNTIME_LOG(Display, TEXT("Dumping Settings for UComboGraphProjectSettings sync points"));
	CG_RUNTIME_LOG(Display, TEXT("\t SyncTypeOnComboTransition=%s"), *UEnum::GetValueAsString(Settings.SyncTypeOnComboTransition));
	CG_RUNTIME_LOG(Display, TEXT("\t SyncTypeOnGraphEnd=%s"), *UEnum::GetValueAsString(Settings.SyncTypeOnGraphEnd));
	CG_RUNTIME_LOG(Display, TEXT("\t SyncTypeOnEventReceived=%s"), *UEnum::GetValueAsString(Settings.SyncTypeOnEventReceived));
}

TOptional<EComboGraphAbilityTaskNetSyncType> UE::ComboGraph::GetSyncTypeConfigValue(const EComboGraphNetworkSyncTime InSyncTime)
{
	TOptional<EComboGraphAbilityTaskNetSyncType> Result;

	const EComboGraphNetworkSyncTypeSetting SyncTypeValue = GetSyncTypeSettingFor(InSyncTime);

	switch (SyncTypeValue)
	{
	case EComboGraphNetworkSyncTypeSetting::None:
		// Case of None, meaning sync point task is not used, and delegate is called immediately
		break;
	case EComboGraphNetworkSyncTypeSetting::BothWait:
		Result = EComboGraphAbilityTaskNetSyncType::BothWait;
		break;
	case EComboGraphNetworkSyncTypeSetting::OnlyServerWait:
		Result = EComboGraphAbilityTaskNetSyncType::OnlyServerWait;
		break;
	case EComboGraphNetworkSyncTypeSetting::OnlyClientWait:
		Result = EComboGraphAbilityTaskNetSyncType::OnlyClientWait;
		break;
	default:
		break;
	}

	return Result;
}

EComboGraphNetworkSyncTypeSetting UE::ComboGraph::GetSyncTypeSettingFor(const EComboGraphNetworkSyncTime InSyncTime)
{
	const UComboGraphProjectSettings& Settings = UComboGraphProjectSettings::Get();
	
	switch (InSyncTime) {
	case EComboGraphNetworkSyncTime::OnComboTransition:
		return Settings.SyncTypeOnComboTransition;
	case EComboGraphNetworkSyncTime::OnGraphEnd:
		return Settings.SyncTypeOnGraphEnd;
	case EComboGraphNetworkSyncTime::OnEventReceived:
		return Settings.SyncTypeOnEventReceived;
	}
	
	return EComboGraphNetworkSyncTypeSetting::None;
}

UComboGraphProjectSettings::UComboGraphProjectSettings()
{
	MontageNodeColor = FLinearColor(100.f / 255.f, 100.f / 255.f, 1.f, 1.f);
	SequenceNodeColor = FLinearColor(80.f / 255.f, 123.f / 255.f, 72.f / 255.f, 1.f);

	DebugActiveColor = FLinearColor(1.f, 0.6f, 0.35f, 1.f);
	DebugFadeTime = 0.75f;

	ContentMargin = FMargin(8.f);
	ContentInternalPadding = FMargin(12.f, 8.f);

	IconSize = 48.f;

	DynamicMontageSlotName = TEXT("DefaultSlot");

	// TODO: Don't forget to update path when plugin gets renamed
	IconsDataTable = FSoftObjectPath(TEXT("/ComboGraph/Xelu_Icons/DT_ComboGraph_InputIconsMappings.DT_ComboGraph_InputIconsMappings"));

	// Make sure a native combo window is added as default
	NotifyStates.Add(UComboGraphANS_ComboWindow::StaticClass(), FComboGraphNotifyStateAutoSetup(0.25f, 0.75f));
}

const UComboGraphProjectSettings& UComboGraphProjectSettings::Get()
{
	const UComboGraphProjectSettings* Settings = GetDefault<UComboGraphProjectSettings>();
	check(Settings);
	return *Settings;
}

UComboGraphProjectSettings& UComboGraphProjectSettings::GetMutable()
{
	UComboGraphProjectSettings* Settings = GetMutableDefault<UComboGraphProjectSettings>();
	check(Settings);
	return *Settings;
}
