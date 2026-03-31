#pragma once

#include "ComboGraphRuntimeTypes.h"
#include "Engine/DeveloperSettings.h"
#include "Layout/Margin.h"
#include "UObject/SoftObjectPtr.h"
#include "ComboGraphProjectSettings.generated.h"

class UAnimNotifyState;
class UDataTable;
class UInputMappingContext;

enum class EComboGraphAbilityTaskNetSyncType : uint8;

UENUM()
enum class EComboGraphNetworkSyncTime: uint8
{
	OnComboTransition,
	OnGraphEnd,
	OnEventReceived
};

UENUM()
enum class EComboGraphNetworkSyncTypeSetting : uint8
{
	/** None (sync point is disabled) */
	None,
	
	/** Both client and server wait until the other signals */
	BothWait,

	/** Only server will wait for the client signal. Client will signal and immediately continue without waiting to hear from Server. */
	OnlyServerWait,

	/** Only client will wait for the server signal. Server will signal and immediately continue without waiting to hear from Client. */
	OnlyClientWait	
};

namespace UE::ComboGraph
{
	/** Outputs to the output log the value of sync point settings */
	void DumpSyncSettings(const TArray<FString>& InArgs);
	
	TOptional<EComboGraphAbilityTaskNetSyncType> GetSyncTypeConfigValue(const EComboGraphNetworkSyncTime InSyncTime);
	EComboGraphNetworkSyncTypeSetting GetSyncTypeSettingFor(const EComboGraphNetworkSyncTime InSyncTime);
}

/** Not used right now but might be. Default behavior is render both */
UENUM()
enum class EComboGraphDrawPinTypes : uint8
{
	/** Setup Event */
	Both UMETA(DisplayName="Draw both Vertical and Horizontal pins"),

	/** Update Event */
	Horizontal UMETA(DisplayName="Draw only Left and Right pins"),

	/** None - invalid */
	Vertical UMETA(DisplayName="Draw only Top and Bottom pins"),
};

UCLASS(Config = Game, defaultconfig)
class COMBOGRAPH_API UComboGraphProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UComboGraphProjectSettings();

	static const UComboGraphProjectSettings& Get();
	static UComboGraphProjectSettings& GetMutable();

#if WITH_EDITOR
	//~ Begin UDeveloperSettings interface
	
	/**
	 * Disable auto registration, we handle register settings manually to register a delegate on settings saved, in ComboGraphModule.
	 *
	 * Namely, to validate and ensure the combo window notify states includes at least one entry.
	 */
	virtual bool SupportsAutoRegistration() const override { return false; }
	//~ End UDeveloperSettings interface
#endif

	/** Default background color for Montage nodes */
	UPROPERTY(config, EditAnywhere, Category = "Colors")
	FLinearColor MontageNodeColor;

	/** Default background color for Sequence nodes */
	UPROPERTY(config, EditAnywhere, Category = "Colors")
	FLinearColor SequenceNodeColor;

	/** Default background color for combo nodes in active states (during debug) */
	UPROPERTY(config, EditAnywhere, Category = "Colors")
	FLinearColor DebugActiveColor;

	/**
	 * The duration used to interpolate the background color of nodes from Active to Default color when active states change (no longer active during debug).
	 *
	 * If set to 0.0, will disable color interpolation.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0"))
	float DebugFadeTime;

	/** The padding around the main node box. */
	UPROPERTY(config, EditAnywhere, Category = "Size & Paddings")
	FMargin ContentMargin;

	/** The padding within the main node box. */
	UPROPERTY(config, EditAnywhere, Category = "Size & Paddings")
	FMargin ContentInternalPadding;

	/** The minimum desired sizes for pin connections. */
	UPROPERTY(config, EditAnywhere, Category = "Size & Paddings", meta = (ClampMin = "20.0"))
	float PinSize = 20.f;

	/** The minimum amount of padding to draw around pin connections. */
	UPROPERTY(config, EditAnywhere, Category = "Size & Paddings", meta = (ClampMin = "0.0"))
	float PinPadding = 20.f;

	// /** The minimum desired sizes for pin connections. */
	// UPROPERTY(config, EditAnywhere, Category = "Size & Paddings", meta = (ClampMin = "0.0"))
	// EComboGraphDrawPinTypes DrawPinTypes = EComboGraphDrawPinTypes::Both;

	/** Enhanced Input Context Mapping to use to draw edge (transition) icons in Graphs */
	UPROPERTY(config, EditAnywhere, Category = "Icons")
	TSoftObjectPtr<UInputMappingContext> ContextMapping;

	/**
	 * Path to the DataTable used to draw edge (transition) icons in Graph. Determine mappings between Keys and Icon textures.
	 *
	 * This is set by default to an internal DataTables (that you can find in `/ComboGraph/Xelu_Icons/`) that setup texture icons for every keyboard and gamepad key.
	 *
	 * Icons are coming from Xelu's Free Controllers & Keyboard Prompts: https://thoseawesomeguys.com/prompts/
	 * Thanks to "Nicolae (Xelu) Berbece" and "Those Awesome Guys" to make it available in the public domain licence under Creative Commons 0 (CC0)
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Icons", meta = (RequiredAssetDataTags = "RowStructure=/Script/ComboGraphEditor.ComboGraphInputsMetaData"))
	TSoftObjectPtr<UDataTable> IconsDataTable;

	/** Icon preference to draw edge (transition) icons in Graph. Can be either Keyboard or Gamepad based */
	UPROPERTY(config, EditDefaultsOnly, Category = "Icons")
	EComboGraphIconType IconPreference = EComboGraphIconType::Keyboard;

	/** Size of Icons when drawing edges (transitions) in Combo Graphs */
	UPROPERTY(config, EditAnywhere, Category = "Icons", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float IconSize;

	/** The Slot Name to use with dynamic montages, created from sequences */
	UPROPERTY(config, EditAnywhere, Category = "Auto Setup")
	FName DynamicMontageSlotName;

	/** Map of Auto Setup Animation Notify States. The key is the Anim Notify State class, the value is the time start / end definition in percent */
	UPROPERTY(config, EditAnywhere, Category = "Auto Setup")
	TMap<TSoftClassPtr<UAnimNotifyState>, FComboGraphNotifyStateAutoSetup> NotifyStates;

	/** Flag to enable / disable message warnings (logs and on screen) about Sequences being used in a networked environment */
	UPROPERTY(config, EditAnywhere, Category = "Warnings", AdvancedDisplay)
	bool bSequencesNetworkedWarning = true;

	/** Flag to disable validation warnings on startup (about missing config for AbilitySystemGlobals). Config writable but not exposed in the editor. */
	UPROPERTY(config)
	bool bIgnoreStartupValidationWarnings = false;
	
	/**
	 * The network sync point type to use when NetworkSyncPoint ability task is used at various points of Combo Graph.
	 *
	 * This config property is used on combo transitions (when a combo graph assets transition from one node to the next).
	 *
	 * Default is None.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Network")
	EComboGraphNetworkSyncTypeSetting SyncTypeOnComboTransition = EComboGraphNetworkSyncTypeSetting::None;
	
	/**
	 * The network sync point type to use when NetworkSyncPoint ability task is used at various points of Combo Graph.
	 *
	 * This config property is used OnGraphEnd (when a combo graph asset is done, just before triggering the OnGraphEnd
	 * Blueprint delegate pin on the ability task).
	 *
	 * Default is None.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Network")
	EComboGraphNetworkSyncTypeSetting SyncTypeOnGraphEnd = EComboGraphNetworkSyncTypeSetting::None;
	
	/**
	 * The network sync point type to use when NetworkSyncPoint ability task is used at various points of Combo Graph.
	 *
	 * This config property is used OnEventReceived (when a gameplay event is received by the owning actor a combo node
	 * is listening to: Effect Containers, Cue Containers, etc.).
	 *
	 * Default is None.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Network")
	EComboGraphNetworkSyncTypeSetting SyncTypeOnEventReceived = EComboGraphNetworkSyncTypeSetting::None;

private:
#if WITH_EDITOR
	void ExportConsoleVariables(FProperty* InProperty);
#endif
};
