// Copyright Impact-Forge. Generalised from the BurningLands reference implementation.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ForgeVehicleMovementInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType)
class UForgeVehicleMovementInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Common contract implemented by every Forge vehicle movement/physics solution (RTune ground physics,
 * the K2 rotary-wing and fixed-wing physics, and the water-craft buoyancy solution). It lets the
 * shared AForgeVehicle base drive any propulsion model uniformly in response to ignition and input,
 * without the base needing to know which physics module actually powers the vehicle.
 */
class FORGEVEHICLESCORE_API IForgeVehicleMovementInterface
{
	GENERATED_BODY()

public:

	/* Spin the propulsion up. Called by the vehicle when its engine ignition completes. */
	virtual void StartEngine() {}

	/* Spin the propulsion down. Called by the vehicle when its engine is cut off. */
	virtual void StopEngine() {}

	/* Returns whether the propulsion is currently producing power. */
	virtual bool IsEngineRunning() const { return false; }

	/* Primary forward/back demand, normalised to [-1, 1] (throttle / collective). */
	virtual void SetThrottleInput(float Value) {}

	/* Left/right demand, normalised to [-1, 1] (steering / yaw / rudder). */
	virtual void SetSteeringInput(float Value) {}

	/* Optional vertical demand, normalised to [-1, 1] (heli collective trim, VTOL, dive planes). */
	virtual void SetVerticalInput(float Value) {}

	/**
	 * Optional roll demand, normalised to [-1, 1]. Aircraft and multirotors need a roll channel
	 * independent of steering; ground and water craft leave this a no-op.
	 *
	 * Named "...AxisInput" rather than "SetRollInput" on purpose: the rotary-wing and fixed-wing
	 * vehicles already declare non-virtual SetRollInput/SetYawInput members, which would silently
	 * hide same-named interface virtuals instead of overriding them.
	 */
	virtual void SetRollAxisInput(float Value) {}

	/**
	 * Optional yaw demand, normalised to [-1, 1], for vehicles whose yaw is a separate channel from
	 * steering (fixed-wing rudder, multirotor yaw). Defaults to routing through SetSteeringInput so
	 * autopilots can drive any vehicle uniformly through this interface.
	 */
	virtual void SetYawAxisInput(float Value) { SetSteeringInput(Value); }
};
