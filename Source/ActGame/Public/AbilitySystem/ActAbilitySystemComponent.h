// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemComponent.h"
#include "Abilities/ActGameplayAbility.h"
#include "NativeGameplayTags.h"
#include "BulletSystemTypes.h"

#include "ActAbilitySystemComponent.generated.h"

class AActor;
class UGameplayAbility;
class UActAbilityTagRelationshipMapping;
class UActGameplayAbility;
class UObject;
struct FFrame;
struct FGameplayAbilityTargetDataHandle;
struct FBulletInitParams;

ACTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_AbilityInputBlocked);

/**
 * UActAbilitySystemComponent
 *
 *	Base ability system component class used by this project.
 */
UCLASS()
class ACTGAME_API UActAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	
	UActAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	typedef TFunctionRef<bool(const UActGameplayAbility* ActAbility, FGameplayAbilitySpecHandle Handle)> TShouldCancelAbilityFunc;
	void CancelAbilitiesByFunc(const TShouldCancelAbilityFunc& ShouldCancelFunc, bool bReplicateCancelAbility);

	void CancelInputActivatedAbilities(bool bReplicateCancelAbility);
	
	void AbilityInputTagPressed(const FGameplayTag InputTag);
	void AbilityInputTagTriggered(const FGameplayTag InputTag);
	void AbilityInputTagReleased(const FGameplayTag InputTag);

	/** Tries to activate one granted ability by class. Returns true only when activation succeeds. */
	bool TryActivateGrantedAbilityByClass(
		TSubclassOf<UActGameplayAbility> AbilityClass,
		bool bAllowRemoteActivation = true,
		bool bCancelIfAlreadyActive = false,
		TSubclassOf<UActGameplayAbility>* OutActivatedAbilityClass = nullptr,
		FGameplayAbilitySpecHandle* OutActivatedSpecHandle = nullptr);

	/** Tries to activate one granted ability by dynamic input tag. */
	bool TryActivateGrantedAbilityByInputTag(
		FGameplayTag InputTag,
		bool bAllowRemoteActivation = true,
		bool bCancelIfAlreadyActive = false,
		TSubclassOf<UActGameplayAbility>* OutActivatedAbilityClass = nullptr,
		FGameplayAbilitySpecHandle* OutActivatedSpecHandle = nullptr);

	/** True if this ASC currently has a granted ability whose class matches (or derives from) AbilityClass. */
	bool HasGrantedAbilityByClass(TSubclassOf<UActGameplayAbility> AbilityClass);

	/** Finds one currently active ability instance by class. */
	bool FindActiveAbilityInstanceByClass(
		TSubclassOf<UActGameplayAbility> AbilityClass,
		TWeakObjectPtr<UActGameplayAbility>& OutAbilityInstance);

	/** True if the specified ability instance is currently active in this ASC. */
	bool IsAbilityInstanceActive(const UActGameplayAbility* AbilityInstance);

	/** Cancel one ability by spec handle (if active). */
	void CancelAbilityByHandle(FGameplayAbilitySpecHandle AbilityHandle);

	/** Rebuilds the AbilityID -> SpecHandle cache from currently granted abilities. */
	void RebuildAbilityIdCache();

	/** Returns the cached spec handle for an AbilityID, or invalid if missing. */
	FGameplayAbilitySpecHandle GetAbilitySpecHandleByID(FName AbilityID) const;

	/** Resolves an AbilityID from an input command tag (InputTag). */
	bool GetAbilityIdByInputTag(FGameplayTag InputTag, FName& OutAbilityId) const;

	/** Tries to activate one granted ability by AbilityID. */
	bool TryActivateAbilityByID(
		FName AbilityID,
		bool bAllowRemoteActivation = true,
		bool bCancelIfAlreadyActive = false,
		FGameplayAbilitySpecHandle* OutActivatedSpecHandle = nullptr);

	UFUNCTION(BlueprintCallable, Category="Act|Ability", DisplayName="AbilityInputTagPressed")
	void K2_AbilityInputTagPressed(const FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category="Act|Ability", DisplayName="AbilityInputTagReleased")
	void K2_AbilityInputTagReleased(const FGameplayTag InputTag);

	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();
	
	bool IsActivationGroupBlocked(EActAbilityActivationGroup Group) const;
	
	/** Sets the current tag relationship mapping, if null it will clear it out */
	void SetTagRelationshipMapping(UActAbilityTagRelationshipMapping* NewMapping);

	/** Looks at ability tags and gathers additional required and blocking tags */
	void GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const;

	void ResetGameplayTagCounts(FGameplayTagContainer TagContainer, const int32 NewCount = 0);
	
	UFUNCTION(BlueprintCallable, Category="Act|Ability", DisplayName="SetGameplayTagCount")
	void K2_SetGameplayTagCount(const FGameplayTag GameplayTag, const int32 NewCount) { SetLooseGameplayTagCount(GameplayTag, NewCount ); }

	UFUNCTION(BlueprintCallable, Category="Act|Ability", DisplayName="ResetGameplayTagCounts")
	void K2_ResetGameplayTagCounts(FGameplayTagContainer TagContainer, const int32 NewCount = 0) {ResetGameplayTagCounts(TagContainer, NewCount ); }

	/** Create a bullet using PawnData->BulletConfig. */
	int32 CreateBullet(FName BulletID, const FBulletInitParams& InitParams) const;

	UFUNCTION(BlueprintCallable, Category="Act|Ability", DisplayName="CreateBullet")
	int32 K2_CreateBullet(FName BulletID, const FBulletInitParams& InitParams);

protected:

	virtual void ClientTryActivateAbility_Implementation(FGameplayAbilitySpecHandle AbilityToActivate) override;
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void OnRep_ActivateAbilities() override;
	
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags) override;
	
protected:

	// If set, this table is used to look up tag relationships for activate and cancel
	UPROPERTY()
	TObjectPtr<UActAbilityTagRelationshipMapping> TagRelationshipMapping;

	/** Cached AbilityID -> SpecHandle map for ID-based activation. */
	TMap<FName, FGameplayAbilitySpecHandle> AbilityIdToSpecHandle;

	// Handles to abilities that had their input pressed this frame.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	// Handles to abilities that had their input released this frame.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// Handles to abilities that have their input held.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	// Number of abilities running in each activation group.
	int32 ActivationGroupCounts[(uint8)EActAbilityActivationGroup::MAX];
};
