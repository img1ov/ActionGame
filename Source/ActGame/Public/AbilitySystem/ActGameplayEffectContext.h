#pragma once

#include "GameplayEffectTypes.h"

#include "ActGameplayEffectContext.generated.h"

class AActor;
class FArchive;
class IActAbilitySourceInterface;
class UObject;
class UPhysicalMaterial;

USTRUCT()
struct FActGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()
	
public:
	
	FActGameplayEffectContext()
		: FGameplayEffectContext()
	{}
	
	FActGameplayEffectContext(AActor* InInstigator, AActor* EffectCauser)
		: FGameplayEffectContext(InInstigator, EffectCauser)
	{}
	
	/** Returns the wrapped FActGameplayEffectContext from the handle, or nullptr if it doesn't exist or is the wrong type */
	static ACTGAME_API FActGameplayEffectContext* ExtractEffectContext(struct FGameplayEffectContextHandle Handle);
	
	/** Sets the object used as the ability source */
	void SetAbilitySource(const IActAbilitySourceInterface* InObject, float InSourceLevel);
	
	/** Returns the ability source interface associated with the source object. Only valid on the authority. */
	const IActAbilitySourceInterface* GetAbilitySource() const;
	
	virtual FGameplayEffectContext* Duplicate() const override
	{
		FActGameplayEffectContext* NewContext = new FActGameplayEffectContext();
		*NewContext = *this;
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FActGameplayEffectContext::StaticStruct();
	}
	
	/** Overridden to serialize new fields */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
	
	/** Returns the physical material from the hit result if there is one */
	const UPhysicalMaterial* GetPhysicalMaterial() const;
	
public:
	
	/** ID to allow the identification of multiple bullets that were part of the same cartridge */
	UPROPERTY()
	int32 CartridgeID = -1;
	
protected:
	
	/** Ability Source object (should implement ILyraAbilitySourceInterface). NOT replicated currently */
	UPROPERTY()
	TWeakObjectPtr<const UObject> AbilitySourceObject;
};

template<>
struct TStructOpsTypeTraits<FActGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FActGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
