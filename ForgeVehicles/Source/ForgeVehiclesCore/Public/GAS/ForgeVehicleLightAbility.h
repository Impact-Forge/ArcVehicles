// Copyright Impact-Forge. Ability that toggles a class of vehicle light.

#pragma once

#include "Components/ForgeVehicleLightComponent.h"
#include "CoreMinimal.h"
#include "GAS/ForgeVehicleAbility.h"

#include "ForgeVehicleLightAbility.generated.h"

/**
 * Toggles every light fixture of a given type on the owning vehicle.
 *
 * Addressing lights by EForgeVehicleLightType (rather than by a bespoke per-light entry point) means
 * one ability class covers headlights, tail lights and navigation strobes by configuration alone,
 * and a light ability can never end up driving the wrong fixture.
 */
UCLASS()
class FORGEVEHICLESCORE_API UForgeVehicleLightAbility : public UForgeVehicleAbility
{
	GENERATED_BODY()

public:

	UForgeVehicleLightAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:

	/* Which fixtures this ability drives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Light")
	EForgeVehicleLightType LightType = EForgeVehicleLightType::Headlight;

	/**
	 * When set, the lights are driven to bDesiredState instead of being toggled. Use for brake
	 * lights, which follow the brake input rather than flipping on each activation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Light")
	bool bSetExplicitState = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Light", meta = (EditCondition = "bSetExplicitState"))
	bool bDesiredState = true;
};
