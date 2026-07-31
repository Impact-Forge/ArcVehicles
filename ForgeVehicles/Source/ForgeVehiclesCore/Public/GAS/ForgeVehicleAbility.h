// Copyright Impact-Forge. Base gameplay ability for vehicle interactions.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"

#include "ForgeVehicleAbility.generated.h"

class AForgeVehicle;

/**
 * Shared base for vehicle gameplay abilities.
 *
 * Server-initiated and instanced per actor, matching how vehicle abilities are granted: the vehicle
 * grants the ability with itself as the spec's SourceObject, so an activating instance can recover
 * which vehicle it belongs to through GetOwningVehicle().
 *
 * Deliberately built on stock UGameplayAbility so the plugin carries no third-party GAS dependency.
 */
UCLASS(Abstract)
class FORGEVEHICLESCORE_API UForgeVehicleAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UForgeVehicleAbility();

protected:

	/* Abilities handed to the activating actor, each granted with SourceObject set to the vehicle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	/**
	 * Resolve the vehicle this ability instance belongs to: the spec's SourceObject if the ability
	 * was granted by a vehicle, otherwise the avatar actor itself (abilities that live on the
	 * vehicle rather than on an occupant).
	 */
	UFUNCTION(BlueprintPure, Category = "ForgeVehicle|Abilities")
	AForgeVehicle* GetOwningVehicle() const;

	/**
	 * Recursively strip collision, physics simulation and movement replication from an actor and
	 * everything attached to it. Used when parking an occupant's body inside a vehicle.
	 *
	 * Note this is deliberately one-way: restoring collision is the responsibility of whoever
	 * un-parks the actor, because only they know which components were meant to be colliding.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeVehicle|Abilities")
	virtual void DisableCollisionForAll(AActor* Actor);

	/* Grants AbilitiesToGrant to the activating actor's ability system, tagged with SourceObject. */
	virtual void GrantSpecificAbilities(const FGameplayAbilityActorInfo* ActorInfo, UObject* SourceObject, const TArray<TSubclassOf<UGameplayAbility>>& AbilitiesToGrant);
};
