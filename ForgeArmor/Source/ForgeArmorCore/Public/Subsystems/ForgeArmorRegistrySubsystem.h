// Copyright Impact-Forge. World registry of armored vehicles + chemical-energy warhead evaluators.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "ForgeArmorTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "ForgeArmorRegistrySubsystem.generated.h"

class UForgeVehicleArmorComponent;
class UForgeWarheadSpec;
class UPrimitiveComponent;

/**
 * Tracks every armored vehicle in the world and evaluates chemical-energy
 * warheads (HEAT / HESH / HE) against them. Kinetic bullet impacts are routed
 * by the ForgeArmorTB module; warhead projectile/explosion actors call the
 * evaluators here from the game thread (server).
 */
UCLASS()
class FORGEARMORCORE_API UForgeArmorRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UForgeArmorRegistrySubsystem* Get(const UObject* WorldContextObject);

	void RegisterArmor(UForgeVehicleArmorComponent* ArmorComponent);
	void UnregisterArmor(UForgeVehicleArmorComponent* ArmorComponent);

	/** Armor component of the actor (registered vehicles only). */
	UFUNCTION(BlueprintCallable, Category = "Forge Armor")
	UForgeVehicleArmorComponent* FindArmorForActor(const AActor* Actor) const;

	/**
	 * Shaped-charge (HEAT) impact against whatever the warhead struck.
	 * Solves the jet through the armor array, applies behind-armor damage and
	 * returns what happened. Returns false when the hit is not an armored
	 * vehicle (caller falls back to its generic explosion).
	 *
	 * @param StandoffMM extra standoff at detonation (fuze distance), mm.
	 */
	UFUNCTION(BlueprintCallable, Category = "Forge Armor", meta = (DisplayName = "Evaluate HEAT Impact"))
	bool EvaluateCEImpact(const FHitResult& HitResult, const UForgeWarheadSpec* Warhead, float StandoffMM, AController* InstigatorController, AActor* DamageCauser, FForgeBehindArmorResult& OutResult);

	/** HESH impact: scab the interior of susceptible plates. */
	UFUNCTION(BlueprintCallable, Category = "Forge Armor", meta = (DisplayName = "Evaluate HESH Impact"))
	bool EvaluateHESHImpact(const FHitResult& HitResult, const UForgeWarheadSpec* Warhead, AController* InstigatorController, AActor* DamageCauser, FForgeBehindArmorResult& OutResult);

	/** HE blast near/at a vehicle: overpressure + external module damage. */
	UFUNCTION(BlueprintCallable, Category = "Forge Armor", meta = (DisplayName = "Evaluate Blast"))
	void EvaluateBlast(const FVector& Origin, const UForgeWarheadSpec* Warhead, AController* InstigatorController, AActor* DamageCauser);

private:
	UPROPERTY()
	TArray<TObjectPtr<UForgeVehicleArmorComponent>> RegisteredArmor;
};
