// Copyright Impact-Forge. Vehicle-generic gameplay attributes (condition, fuel, stowed ammunition).

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"

#include "ForgeVehicleAttributeSet.generated.h"

/** Standard GAS accessor boilerplate (getter/setter/initter per attribute). */
#define FORGE_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Attributes every powered vehicle shares: structural condition, fuel and stowed ammunition.
 *
 * Deliberately built on stock UAttributeSet rather than a GAS-wrapper plugin's base class, so
 * ForgeVehicles stays consumable by any project without dragging in a third-party GAS layer.
 *
 * Health here is the vehicle's own aggregate condition. Per-module damage (engine, tracks, crew,
 * ammo racks) is modelled separately by the ForgeArmor plugin, which bridges its aggregate
 * structural loss into this Health attribute through a set-by-caller gameplay effect.
 */
UCLASS()
class FORGEVEHICLESCORE_API UForgeVehicleAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:

	UForgeVehicleAttributeSet();

	//~ Begin UAttributeSet interface
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// End UAttributeSet interface

	/* Structural condition. Reaching zero is the vehicle's death condition. */
	UPROPERTY(BlueprintReadOnly, Category = "ForgeVehicle|Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health = 100.f;
	FORGE_ATTRIBUTE_ACCESSORS(UForgeVehicleAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "ForgeVehicle|Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth = 100.f;
	FORGE_ATTRIBUTE_ACCESSORS(UForgeVehicleAttributeSet, MaxHealth)

	/* Rounds carried for mounted armament. */
	UPROPERTY(BlueprintReadOnly, Category = "ForgeVehicle|Attributes", ReplicatedUsing = OnRep_StowedAmmunition)
	FGameplayAttributeData StowedAmmunition = 300.f;
	FORGE_ATTRIBUTE_ACCESSORS(UForgeVehicleAttributeSet, StowedAmmunition)

	UPROPERTY(BlueprintReadOnly, Category = "ForgeVehicle|Attributes", ReplicatedUsing = OnRep_MaxStowedAmmunition)
	FGameplayAttributeData MaxStowedAmmunition = 300.f;
	FORGE_ATTRIBUTE_ACCESSORS(UForgeVehicleAttributeSet, MaxStowedAmmunition)

	/* Remaining fuel/charge. Drone battery endurance uses its own component, not this attribute. */
	UPROPERTY(BlueprintReadOnly, Category = "ForgeVehicle|Attributes", ReplicatedUsing = OnRep_Fuel)
	FGameplayAttributeData Fuel = 500.f;
	FORGE_ATTRIBUTE_ACCESSORS(UForgeVehicleAttributeSet, Fuel)

	UPROPERTY(BlueprintReadOnly, Category = "ForgeVehicle|Attributes", ReplicatedUsing = OnRep_MaxFuel)
	FGameplayAttributeData MaxFuel = 500.f;
	FORGE_ATTRIBUTE_ACCESSORS(UForgeVehicleAttributeSet, MaxFuel)

protected:

	/* Scales a current value so it keeps its proportion when its maximum changes. */
	void AdjustAttributeForMaxChange(const FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty) const;

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_StowedAmmunition(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxStowedAmmunition(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_Fuel(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxFuel(const FGameplayAttributeData& OldValue);
};
