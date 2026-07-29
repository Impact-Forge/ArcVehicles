// Copyright Impact-Forge. Water-craft physics authored for Forge Vehicles.

#pragma once

#include "CoreMinimal.h"
#include "ForgeVehicle.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "ForgeWaterCraft.generated.h"

class UStaticMeshComponent;
class UForgeBuoyancyComponent;

/**
 * Forge water craft (boats, hovercraft, small ships).
 *
 * Authored to match the design of the other Forge vehicle modules: it derives from AForgeVehicle so
 * it inherits the Arc seat / exit / ignition foundation, implements IForgeVehicleMovementInterface so
 * the shared base routes throttle & steering into propulsion, and drives a simulating hull primitive
 * with a UForgeBuoyancyComponent for flotation plus force/torque based thrust and turning.
 */
UCLASS()
class FORGEVEHICLESWATERCRAFT_API AForgeWaterCraft : public AForgeVehicle, public IForgeVehicleMovementInterface
{
	GENERATED_BODY()

public:

	AForgeWaterCraft(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin AActor interface
	virtual void Tick(float DeltaTime) override;
	// End AActor interface

	//~ Begin IForgeVehicleMovementInterface
	virtual void SetThrottleInput(float Value) override { ThrottleInput = FMath::Clamp(Value, -1.f, 1.f); }
	virtual void SetSteeringInput(float Value) override { SteeringInput = FMath::Clamp(Value, -1.f, 1.f); }
	virtual void StartEngine() override { bEngineRunning = true; }
	virtual void StopEngine() override { bEngineRunning = false; ThrottleInput = 0.f; SteeringInput = 0.f; }
	virtual bool IsEngineRunning() const override { return bEngineRunning; }
	//~ End IForgeVehicleMovementInterface

	UStaticMeshComponent* GetHull() const { return Hull; }
	UForgeBuoyancyComponent* GetBuoyancy() const { return Buoyancy; }

public:

	/* Simulating hull body. Root component of the water craft. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* Hull;

	/* Flotation solver acting on the hull. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UForgeBuoyancyComponent* Buoyancy;

protected:

	/* Forward thrust applied at full throttle (uu * kg / s^2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WaterCraft|Propulsion", meta = (ClampMin = "0.0"))
	float ThrustForce = 45000.f;

	/* Yaw torque applied at full steering (uu * kg / s^2), scaled by forward speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WaterCraft|Propulsion", meta = (ClampMin = "0.0"))
	float TurnTorque = 30000.f;

	/* Speed (cm/s) above which no further thrust is applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WaterCraft|Propulsion", meta = (ClampMin = "0.0"))
	float MaxSpeed = 1600.f;

	/* Minimum fraction of turn authority available when stationary (so the craft can still be pointed). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WaterCraft|Propulsion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinTurnAuthority = 0.15f;

	/* Speed (cm/s) at which full turn authority is reached. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WaterCraft|Propulsion", meta = (ClampMin = "1.0"))
	float FullTurnAuthoritySpeed = 300.f;

	/* Replicated to clients so their non-authoritative Tick can present propulsion effects. */
	UPROPERTY(BlueprintReadOnly, Category = "WaterCraft")
	bool bEngineRunning = false;

	float ThrottleInput = 0.f;
	float SteeringInput = 0.f;
};
