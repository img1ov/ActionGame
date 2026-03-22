#include "AbilitySystem/ActGameplayEffectContext.h"

#include "AbilitySystem/ActAbilitySourceInterface.h"
#include "Engine/HitResult.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Serialization/GameplayEffectContextNetSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActGameplayEffectContext)

class FArchive;

FActGameplayEffectContext* FActGameplayEffectContext::ExtractEffectContext(struct FGameplayEffectContextHandle Handle)
{
	FGameplayEffectContext* BaseEffectContext = Handle.Get();
	if ((BaseEffectContext != nullptr) && BaseEffectContext->GetScriptStruct()->IsChildOf(FActGameplayEffectContext::StaticStruct()))
	{
		return static_cast<FActGameplayEffectContext*>(BaseEffectContext);
	}
	
	return nullptr;
}

void FActGameplayEffectContext::SetAbilitySource(const IActAbilitySourceInterface* InObject, float InSourceLevel)
{
	AbilitySourceObject = MakeWeakObjectPtr(Cast<const UObject>(InObject));
	//SourceLevel = InSourceLevel;
}

const IActAbilitySourceInterface* FActGameplayEffectContext::GetAbilitySource() const
{
	return Cast<IActAbilitySourceInterface>(AbilitySourceObject.Get());
}

bool FActGameplayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);

	// Not serialized for post-activation use:
	// CartridgeID

	return true;
}

namespace UE::Net
{
	// Forward to FGameplayEffectContextNetSerializer
	// Note: If FActGameplayEffectContext::NetSerialize() is modified, a custom NetSerializer must be implemented as the current fallback will no longer be sufficient.
	UE_NET_IMPLEMENT_FORWARDING_NETSERIALIZER_AND_REGISTRY_DELEGATES(ActGameplayEffectContext, FGameplayEffectContextNetSerializer);
}

const UPhysicalMaterial* FActGameplayEffectContext::GetPhysicalMaterial() const
{
	if (const FHitResult* HitResultPtr = GetHitResult())
	{
		return HitResultPtr->PhysMaterial.Get();
	}
	
	return nullptr;
}
