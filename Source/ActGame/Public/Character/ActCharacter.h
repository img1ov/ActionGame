// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ModularCharacter.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagAssetInterface.h"
#include "Teams/ActTeamAgentInterface.h"
#include "BulletSystemInterface.h"

#include "ActCharacter.generated.h"

#define UE_API ACTGAME_API

class UActCharacterMovementComponent;
class UActCameraComponent;
class UActSpringArmComponent;
class UActBattleComponent;
class AActPlayerState;
class AActPlayerController;
class UActPawnData;
class UActHealthComponent;
class UActPawnExtensionComponent;
class UActAbilitySystemComponent;
class UBulletSystemComponent;
struct FFrame;
struct FGameplayTag;
struct FGameplayTagContainer;

USTRUCT()
struct FActReplicatedAcceleration
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 AccelXYRadians = 0;	// Direction of XY accel component, quantized to represent [0, 2*pi]

	UPROPERTY()
	uint8 AccelXYMagnitude = 0;	//Accel rate of XY component, quantized to represent [0, MaxAcceleration]

	UPROPERTY()
	int8 AccelZ = 0;	// Raw Z accel rate component, quantized to represent [-MaxAcceleration, MaxAcceleration]
};

USTRUCT()
struct FSharedRepMovement
{
	GENERATED_BODY()

	FSharedRepMovement();

	bool FillForCharacter(ACharacter* Character);
	bool Equals(const FSharedRepMovement& Other, ACharacter* Character) const;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	UPROPERTY(Transient)
	FRepMovement RepMovement;

	UPROPERTY(Transient)
	float RepTimeStamp = 0.0f;

	UPROPERTY(Transient)
	uint8 RepMovementMode = 0;

	UPROPERTY(Transient)
	bool bProxyIsJumpForceApplied = false;

	UPROPERTY(Transient)
	bool bIsCrouched = false;
};

template<>
struct TStructOpsTypeTraits<FSharedRepMovement> : public TStructOpsTypeTraitsBase2<FSharedRepMovement>
{
	enum
	{
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
	};
};

/**
 * AActCharacter
 *
 *	The base character pawn class used by this project.
 *	Responsible for sending events to pawn components.
 *	New behavior should be added via pawn components when possible.
 */
UCLASS(MinimalAPI, Config = Game, Meta = (ShortTooltip = "The base character pawn class used by this project."))
class AActCharacter : public AModularCharacter, public IAbilitySystemInterface, public IGameplayTagAssetInterface, public IActTeamAgentInterface, public IBulletSystemInterface
{
	GENERATED_BODY()

public:
	
	UE_API AActCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Act|Character")
	UE_API AActPlayerController* GetActPlayerController() const;

	UFUNCTION(BlueprintCallable, Category = "Act|Character")
	UE_API AActPlayerState* GetActPlayerState() const;
	
	UFUNCTION(BlueprintCallable, Category = "Act|Character")
	UE_API UActCharacterMovementComponent* GetActMovementComponent() const;

	UE_API virtual UActAbilitySystemComponent* GetActAbilitySystemComponent() const;
	UE_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UE_API virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	UE_API virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	UE_API virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	UE_API virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	
	UE_API void ToggleCrouch();

	UE_API virtual UBulletSystemComponent* GetBulletSystemComponent_Implementation() const override;

	//~AActor interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void Reset() override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UE_API virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	//~End of AActor interface

	//~APawn interface
	UE_API virtual void NotifyControllerChanged() override;
	//~End of APawn interface
	
	//~IActTeamAgentInterface interface
	UE_API virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	UE_API virtual FGenericTeamId GetGenericTeamId() const override;
	UE_API virtual FOnActTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	//~End of IActTeamAgentInterface interface
	
	/** RPCs that is called on frames when default property replication is skipped. This replicates a single movement update to everyone. */
	UFUNCTION(NetMulticast, unreliable)
	UE_API void FastSharedReplication(const FSharedRepMovement& SharedRepMovement);
	
	// Last FSharedRepMovement we sent, to avoid sending repeatedly.
	FSharedRepMovement LastSharedReplication;
	
	virtual bool UpdateSharedReplication();

protected:

	UE_API virtual void OnAbilitySystemInitialized();
	UE_API virtual void OnAbilitySystemUninitialized();

	UE_API virtual void PossessedBy(AController* NewController) override;
	UE_API virtual void UnPossessed() override;

	UE_API virtual void OnRep_Controller() override;
	UE_API virtual void OnRep_PlayerState() override;

	UE_API virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UE_API void InitializeGameplayTags();

	UE_API virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

	// Begins the death sequence for the character (disables collision, disables movement, etc...)
	UFUNCTION()
	UE_API virtual void OnDeathStarted(AActor* OwningActor);

	// Ends the death sequence for the character (detaches controller, destroys pawn, etc...)
	UFUNCTION()
	UE_API virtual void OnDeathFinished(AActor* OwningActor);

	// Called when the death sequence for the character has completed
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnDeathFinished"))
	UE_API void K2_OnDeathFinished();

	UE_API void DisableMovementAndCollision();
	UE_API void DestroyDueToDeath();
	UE_API void UninitAndDestroy();

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	UE_API void SetMovementModeTag(EMovementMode MovementMode, uint8 CustomMovementMode, bool bTagEnabled);
	
	UE_API virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	UE_API virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	
	UE_API virtual bool CanJumpInternal_Implementation() const override;
	
private:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Act|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActPawnExtensionComponent> PawnExtComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Act|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActBattleComponent> BattleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Act|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBulletSystemComponent> BulletSystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Act|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Act|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActSpringArmComponent> CameraSpringArmComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Act|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActCameraComponent> CameraComponent;
	
	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedAcceleration)
	FActReplicatedAcceleration ReplicatedAcceleration;
	
	UPROPERTY(ReplicatedUsing = OnRep_MyTeamID)
	FGenericTeamId MyTeamID;
	
	UPROPERTY()
	FOnActTeamIndexChangedDelegate OnTeamChangedDelegate;
	
protected:
	
	// Called to determine what happens to the team ID when possession ends
	virtual FGenericTeamId DetermineNewTeamAfterPossessionEnds(FGenericTeamId OldTeamID) const
	{
		// This could be changed to return, e.g., OldTeamID if you want to keep it assigned afterwards, or return an ID for some neutral faction, or etc...
		return FGenericTeamId::NoTeam;
	}
	
private:
	
	UFUNCTION()
	UE_API void OnControllerChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);
	
	UFUNCTION()
	UE_API void OnRep_ReplicatedAcceleration();
	
	UFUNCTION()
	UE_API void OnRep_MyTeamID(FGenericTeamId OldTeamID);
	
};

#undef UE_API
