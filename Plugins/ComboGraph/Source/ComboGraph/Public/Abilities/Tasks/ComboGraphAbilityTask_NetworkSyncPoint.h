#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "ComboGraphAbilityTask_NetworkSyncPoint.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FComboGraphNetworkSyncDelegate);

UENUM()
enum class EComboGraphAbilityTaskNetSyncType : uint8
{
	/** Both client and server wait until the other signals */
	BothWait,

	/** Only server will wait for the client signal. Client will signal and immediately continue without waiting to hear from Server. */
	OnlyServerWait,

	/** Only client will wait for the server signal. Server will signal and immediately continue without waiting to hear from Client. */
	OnlyClientWait	
};

/**
 * Task for providing a generic sync point for client server (one can wait for a signal from the other)
 *
 * Originally forked from engine to fix an issue with 5.1.0 (https://github.com/EpicGames/UnrealEngine/commit/49b4d85a967a1093f188334d9b1be65805ec00e8)
 *
 * The only difference now with UAbilityTask_NetworkSyncPoint is the addition of OnSyncDelegate to register a delegate when used
 * in C++ tasks (in StartGraph or PlayMontage combo graph tasks).
 */
UCLASS()
class COMBOGRAPH_API UComboGraphAbilityTask_NetworkSyncPoint : public UAbilityTask
{
	GENERATED_UCLASS_BODY()
	
	DECLARE_MULTICAST_DELEGATE(FNetworkSyncNativeDelegate);
	
	UPROPERTY(BlueprintAssignable)
	FComboGraphNetworkSyncDelegate OnSync;
	
	/** Native delegate on sync */
	FNetworkSyncNativeDelegate OnSyncDelegate;

	UFUNCTION()
	void OnSignalCallback();

	virtual void Activate() override;

	/**
	 *	
	 *	Synchronize execution flow between Client and Server. Depending on SyncType, the Client and Server will wait for the other to reach this node or another WaitNetSync node in the ability before continuing execution.  
	 *	
	 *	BothWait - Both Client and Server will wait until the other reaches the node. (Whoever gets their first, waits for the other before continueing).
	 *	OnlyServerWait - Only server will wait for the client signal. Client will signal and immediately continue without waiting to hear from Server.
	 *	OnlyClientWait - Only client will wait for the server signal. Server will signal and immediately continue without waiting to hear from Client.
	 *	
	 *	Note that this is "ability instance wide". These sync points never affect sync points in other abilities. 
	 *	
	 *	In most cases you will have both client and server execution paths connected to the same WaitNetSync node. However it is possible to use separate nodes
	 *	for cleanliness of the graph. The "signal" is "ability instance wide".
	 *	
	 */
	static UComboGraphAbilityTask_NetworkSyncPoint* WaitNetSync(UGameplayAbility* OwningAbility, EComboGraphAbilityTaskNetSyncType SyncType);

protected:
	void SyncFinished();

	/** The event we replicate */
	EAbilityGenericReplicatedEvent::Type ReplicatedEventToListenFor;

	EComboGraphAbilityTaskNetSyncType SyncType;
};
