// Copyright Impact-Forge. Hand/catapult-launched fixed-wing reconnaissance UAV.

#pragma once

#include "CoreMinimal.h"
#include "ForgeFixedWingVehicle.h"

#include "ForgeFixedWingUAV.generated.h"

class UForgeDroneAutopilotComponent;
class UForgeDroneBatteryComponent;
class UForgeDroneLinkComponent;
class UForgeDroneOperatorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnForgeUAVLaunched);

/**
 * Small fixed-wing reconnaissance drone of the hand- or catapult-launched class.
 *
 * The aerodynamics come straight from AForgeFixedWingVehicle - its model is dimensionally sound SI, so
 * it scales down to a 2 kg airframe by retuning rather than reimplementation. What this class adds is
 * what a small UAV needs and a crewed aeroplane does not: it has no undercarriage and no runway, so it
 * starts life as an impulse from someone's arm or a bungee, and it earns its keep by loitering
 * unattended rather than being flown continuously.
 *
 * Engine thrust is authored on the Blueprint's UForgeAeroEngineComponent (see the module README for
 * the tuned figures); this class owns the airframe, launch and payload wiring.
 */
UCLASS()
class FORGEVEHICLESDRONES_API AForgeFixedWingUAV : public AForgeFixedWingVehicle
{
	GENERATED_BODY()

public:

	AForgeFixedWingUAV(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	/* Airframe mass, kg. Applied as an explicit override before the aero model reads it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeUAV", meta = (ClampMin = "0.1"))
	float AirframeMassKg = 2.f;

	/* Speed imparted by the launch, m/s. Must exceed stall speed or the aircraft simply falls. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeUAV|Launch", meta = (ClampMin = "1.0"))
	float LaunchSpeedMS = 12.f;

	/* Upward component of the launch, degrees above the facing direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeUAV|Launch", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float LaunchPitchDeg = 15.f;

	/* Throttle applied on launch so the aircraft accelerates away instead of decaying into a stall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeUAV|Launch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LaunchThrottle = 1.f;

	/* Begin an orbit at the launch point once airborne, so the drone is useful unattended. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeUAV|Launch")
	bool bOrbitAfterLaunch = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeUAV|Launch", meta = (EditCondition = "bOrbitAfterLaunch", ClampMin = "10.0"))
	float PostLaunchOrbitRadiusM = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeUAV|Launch", meta = (EditCondition = "bOrbitAfterLaunch", ClampMin = "10.0"))
	float PostLaunchOrbitAltitudeM = 80.f;

	UPROPERTY(BlueprintAssignable, Category = "ForgeUAV|Launch")
	FOnForgeUAVLaunched OnLaunched;

	/* Throw the aircraft into the air and spin the motor up. Server authoritative. */
	UFUNCTION(BlueprintCallable, Category = "ForgeUAV|Launch")
	void Launch();

	UFUNCTION(BlueprintPure, Category = "ForgeUAV|Launch")
	bool HasLaunched() const { return bLaunched; }

	UForgeDroneBatteryComponent* GetBattery() const { return Battery; }
	UForgeDroneLinkComponent* GetLink() const { return Link; }
	UForgeDroneAutopilotComponent* GetAutopilot() const { return Autopilot; }
	UForgeDroneOperatorComponent* GetOperator() const { return Operator; }

protected:

	/* Applies the small-airframe rescales to the inherited aero model. */
	virtual void ApplySmallAirframeTuning();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneBatteryComponent> Battery;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneLinkComponent> Link;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneAutopilotComponent> Autopilot;

	/* How a player takes and gives up control of the aircraft. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneOperatorComponent> Operator;

	UPROPERTY(Replicated)
	bool bLaunched = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
