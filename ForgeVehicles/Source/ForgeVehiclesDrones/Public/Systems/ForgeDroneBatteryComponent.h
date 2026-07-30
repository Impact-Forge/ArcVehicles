// Copyright Impact-Forge. Drone battery: capacity, draw and endurance.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "ForgeDroneBatteryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeBatteryChargeChanged, float, ChargeFraction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnForgeBatteryThreshold);

/**
 * Flight battery for an electric drone.
 *
 * Endurance is the defining constraint of drone warfare: a quad is only over the target for as long
 * as its pack lasts, and flying hard shortens that sharply because propeller shaft power rises
 * faster than thrust. The component integrates real draw against a watt-hour capacity rather than
 * running a flat timer, so hovering, sprinting and climbing cost visibly different amounts.
 *
 * When the pack empties, the airframe loses power: a multirotor falls, a fixed-wing glides.
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESDRONES_API UForgeDroneBatteryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeDroneBatteryComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// End UActorComponent interface

	/* Usable pack capacity, watt-hours. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Battery", meta = (ClampMin = "0.1"))
	float CapacityWh = 28.f;

	/* Constant draw from flight controller, radio, camera and payload electronics, watts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Battery", meta = (ClampMin = "0.0"))
	float AvionicsLoadW = 8.f;

	/* Draw at full throttle on every motor, watts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Battery", meta = (ClampMin = "0.0"))
	float MaxPropulsionLoadW = 1400.f;

	/* Charge fraction at which OnLowBattery fires (warning, still flyable). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Battery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowChargeThreshold = 0.20f;

	/* Charge fraction at which OnCriticalBattery fires (come home now). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Battery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CriticalChargeThreshold = 0.05f;

	/* Starting charge as a fraction of capacity, so a partly-used pack can be deployed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Battery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialChargeFraction = 1.f;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Battery")
	FOnForgeBatteryChargeChanged OnChargeChanged;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Battery")
	FOnForgeBatteryThreshold OnLowBattery;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Battery")
	FOnForgeBatteryThreshold OnCriticalBattery;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Battery")
	FOnForgeBatteryThreshold OnDepleted;

	/* Remaining charge, 0-1. Replicated for HUD and link telemetry. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Battery")
	float GetChargeFraction() const { return ChargeFraction; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Battery")
	float GetRemainingWh() const { return ChargeFraction * CapacityWh; }

	/* Estimated flight time left at the current draw, minutes. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Battery")
	float GetEstimatedEnduranceMinutes() const;

	/* Instantaneous total draw, watts. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Battery")
	float GetCurrentLoadW() const { return CurrentLoadW; }

	/* Additional constant draw from an active payload (gimbal, jammer, illuminator). */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Battery")
	void SetPayloadLoadW(float NewPayloadLoadW);

	/* Swap in a fresh pack (rearm, battery change). Server authoritative. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Battery")
	void Recharge(float NewChargeFraction = 1.f);

protected:

	UFUNCTION()
	void OnRep_ChargeFraction();

	/* Reads the owner's current propulsion demand. Multirotors report mean motor output. */
	float GatherPropulsionDemand() const;

	/* Pushes remaining power onto the airframe, so an empty pack actually stops the motors. */
	void ApplyPowerScaleToOwner(float PowerScale) const;

	UPROPERTY(ReplicatedUsing = OnRep_ChargeFraction)
	float ChargeFraction = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Battery", meta = (ClampMin = "0.0"))
	float PayloadLoadW = 0.f;

	float CurrentLoadW = 0.f;

	bool bLowReported = false;
	bool bCriticalReported = false;
	bool bDepletedReported = false;
};
