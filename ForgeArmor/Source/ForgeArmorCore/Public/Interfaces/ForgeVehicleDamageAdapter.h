// Copyright Impact-Forge. Adapter contract mapping module damage onto a concrete vehicle implementation.

#pragma once

#include "CoreMinimal.h"
#include "ForgeArmorTypes.h"
#include "UObject/Interface.h"

#include "ForgeVehicleDamageAdapter.generated.h"

class APawn;

UINTERFACE(BlueprintType, MinimalAPI)
class UForgeVehicleDamageAdapter : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by a vehicle (or an adapter component on it) to translate abstract
 * module damage into concrete vehicle behaviour. The damage component discovers
 * the adapter on its owner (actor first, then components) and calls these on the
 * server; state that must affect clients is the adapter's responsibility to
 * replicate (or derive from the replicated module states).
 *
 * All events have empty default implementations - implement only what the
 * vehicle supports.
 */
class FORGEARMORCORE_API IForgeVehicleDamageAdapter
{
	GENERATED_BODY()

public:
	/** Engine output scale (1 = healthy, 0 = dead) and hard-kill flag. */
	UFUNCTION(BlueprintNativeEvent, Category = "Forge Armor")
	void ApplyEngineState(float PowerScale, bool bKilled);

	/** Per-wheel damage: stiffness scale for degraded tires, disabled = blown off. */
	UFUNCTION(BlueprintNativeEvent, Category = "Forge Armor")
	void ApplyWheelState(int32 WheelIndex, float StiffnessScale, bool bDisabled);

	/** Track thrown or destroyed on one side (tracked vehicles). */
	UFUNCTION(BlueprintNativeEvent, Category = "Forge Armor")
	void ApplyTrackState(bool bLeftTrack, bool bDisabled);

	/** Turret drive degradation: traverse/elevation speed scales (0 = jammed). */
	UFUNCTION(BlueprintNativeEvent, Category = "Forge Armor")
	void ApplyTurretDriveState(float TraverseScale, float ElevationScale);

	/** Main gun state: false = breech/barrel destroyed, cannot fire. */
	UFUNCTION(BlueprintNativeEvent, Category = "Forge Armor")
	void ApplyGunState(bool bCanFire);

	/** Resolve the pawn occupying a seat (INDEX_NONE when empty). */
	UFUNCTION(BlueprintNativeEvent, Category = "Forge Armor")
	APawn* GetCrewOccupant(int32 SeatIndex) const;

	/** Deliver a crew wound (spall fragment, jet, fire) to the seat occupant. */
	UFUNCTION(BlueprintNativeEvent, Category = "Forge Armor")
	void ApplyCrewWound(const FForgeCrewWound& Wound);

	/** Vehicle-level kill state transitions (wreck handling, seat lockout, FX). */
	UFUNCTION(BlueprintNativeEvent, Category = "Forge Armor")
	void NotifyKillStateChanged(EForgeVehicleKillState NewState);
};
