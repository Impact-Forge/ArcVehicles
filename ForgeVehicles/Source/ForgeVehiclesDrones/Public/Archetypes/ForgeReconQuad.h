// Copyright Impact-Forge. Hover-stable reconnaissance quadcopter.

#pragma once

#include "CoreMinimal.h"
#include "Flight/ForgeMultirotorVehicle.h"

#include "ForgeReconQuad.generated.h"

class UForgeDroneBatteryComponent;
class UForgeDroneGimbalComponent;
class UForgeDroneLinkComponent;
class UForgeDroneAutopilotComponent;
class UForgeDroneDropReleaseComponent;

/**
 * Camera quadcopter of the commercial/military-adapted class: stable, long-endurance, flown to watch
 * rather than to fight.
 *
 * Everything here is tuned for a steady picture instead of agility. It flies in Angle mode with a
 * modest tilt limit, so releasing the sticks parks it in a hover, and it carries a stabilised gimbal
 * because a fixed camera on a manoeuvring airframe is useless for observation. Long endurance and a
 * long control link matter more than speed: this drone's job is to sit still a long way from its
 * operator. It can carry a couple of small stores, which is the field-improvised role these airframes
 * are actually put to.
 */
UCLASS()
class FORGEVEHICLESDRONES_API AForgeReconQuad : public AForgeMultirotorVehicle
{
	GENERATED_BODY()

public:

	AForgeReconQuad(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	/* Airframe mass, kg. Applied as an explicit override before the flight model reads it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeReconQuad", meta = (ClampMin = "0.05"))
	float AirframeMassKg = 0.92f;

	UForgeDroneBatteryComponent* GetBattery() const { return Battery; }
	UForgeDroneLinkComponent* GetLink() const { return Link; }
	UForgeDroneGimbalComponent* GetGimbal() const { return Gimbal; }
	UForgeDroneAutopilotComponent* GetAutopilot() const { return Autopilot; }
	UForgeDroneDropReleaseComponent* GetStores() const { return Stores; }

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneBatteryComponent> Battery;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneLinkComponent> Link;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneGimbalComponent> Gimbal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneAutopilotComponent> Autopilot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneDropReleaseComponent> Stores;
};
