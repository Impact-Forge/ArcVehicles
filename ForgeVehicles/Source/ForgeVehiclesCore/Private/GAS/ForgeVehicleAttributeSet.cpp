// Copyright Impact-Forge. Vehicle attribute set implementation.

#include "GAS/ForgeVehicleAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UForgeVehicleAttributeSet::UForgeVehicleAttributeSet()
{
}

void UForgeVehicleAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// REPNOTIFY_Always so UI bound to these attributes still updates when a value is set back to
	// the same number it already had (e.g. a refill that lands exactly on the previous value).
	DOREPLIFETIME_CONDITION_NOTIFY(UForgeVehicleAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UForgeVehicleAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UForgeVehicleAttributeSet, StowedAmmunition, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UForgeVehicleAttributeSet, MaxStowedAmmunition, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UForgeVehicleAttributeSet, Fuel, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UForgeVehicleAttributeSet, MaxFuel, COND_None, REPNOTIFY_Always);
}

void UForgeVehicleAttributeSet::AdjustAttributeForMaxChange(const FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, const float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty) const
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();

	if (!ASC || FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue))
	{
		return;
	}

	// Preserve the current/max ratio across the maximum changing (an upgrade that raises MaxFuel
	// shouldn't leave the vehicle proportionally emptier than it was).
	const float CurrentValue = AffectedAttribute.GetCurrentValue();
	const float NewDelta = (CurrentMaxValue > UE_SMALL_NUMBER)
		? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue
		: NewMaxValue;

	ASC->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
}

void UForgeVehicleAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		AdjustAttributeForMaxChange(Health, MaxHealth, NewValue, GetHealthAttribute());
	}
	else if (Attribute == GetMaxStowedAmmunitionAttribute())
	{
		AdjustAttributeForMaxChange(StowedAmmunition, MaxStowedAmmunition, NewValue, GetStowedAmmunitionAttribute());
	}
	else if (Attribute == GetMaxFuelAttribute())
	{
		AdjustAttributeForMaxChange(Fuel, MaxFuel, NewValue, GetFuelAttribute());
	}
}

void UForgeVehicleAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Clamp after every effect execution so no caller can drive a value out of range.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetStowedAmmunitionAttribute())
	{
		SetStowedAmmunition(FMath::Clamp(GetStowedAmmunition(), 0.f, GetMaxStowedAmmunition()));
	}
	else if (Data.EvaluatedData.Attribute == GetFuelAttribute())
	{
		SetFuel(FMath::Clamp(GetFuel(), 0.f, GetMaxFuel()));
	}
}

void UForgeVehicleAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UForgeVehicleAttributeSet, Health, OldValue);
}

void UForgeVehicleAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UForgeVehicleAttributeSet, MaxHealth, OldValue);
}

void UForgeVehicleAttributeSet::OnRep_StowedAmmunition(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UForgeVehicleAttributeSet, StowedAmmunition, OldValue);
}

void UForgeVehicleAttributeSet::OnRep_MaxStowedAmmunition(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UForgeVehicleAttributeSet, MaxStowedAmmunition, OldValue);
}

void UForgeVehicleAttributeSet::OnRep_Fuel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UForgeVehicleAttributeSet, Fuel, OldValue);
}

void UForgeVehicleAttributeSet::OnRep_MaxFuel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UForgeVehicleAttributeSet, MaxFuel, OldValue);
}
