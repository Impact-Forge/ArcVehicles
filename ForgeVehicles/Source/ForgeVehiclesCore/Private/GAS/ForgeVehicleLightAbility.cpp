// Copyright Impact-Forge. Vehicle light ability implementation.

#include "GAS/ForgeVehicleLightAbility.h"

#include "ForgeVehicle.h"

UForgeVehicleLightAbility::UForgeVehicleLightAbility()
{
}

void UForgeVehicleLightAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}

	if (AForgeVehicle* Vehicle = GetOwningVehicle())
	{
		if (bSetExplicitState)
		{
			Vehicle->SetLightsOfType(LightType, bDesiredState);
		}
		else
		{
			Vehicle->ToggleLightsOfType(LightType);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}
