// Copyright Impact-Forge. World wind field sampled by light airframes.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ForgeWindSubsystem.generated.h"

/**
 * A simple world wind field: a steady base vector plus deterministic gusting.
 *
 * Wind is what makes small drones feel small. A 900 g quad holding station in a breeze visibly
 * crabs into it, and a hand-launched fixed-wing has to fight for altitude on the upwind leg. Because
 * the force scales with drag area over mass, the same wind that shoves a drone around is irrelevant
 * to a tank, so only airframes that opt in ever sample it.
 *
 * Sampling is a pure function of position and time, so clients can sample it for cosmetics (dust,
 * foliage, tracer drift) and get the same answer as the server without replicating anything.
 */
UCLASS()
class FORGEVEHICLESDRONES_API UForgeWindSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	static UForgeWindSubsystem* Get(const UObject* WorldContextObject);

	/* Steady wind velocity in cm/s, world space. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Wind")
	void SetBaseWind(const FVector& NewBaseWindCmS) { BaseWindCmS = NewBaseWindCmS; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Wind")
	FVector GetBaseWind() const { return BaseWindCmS; }

	/* Convenience setter: compass heading the wind blows *towards*, degrees, and speed in m/s. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Wind")
	void SetWindFromHeading(float HeadingDegrees, float SpeedMS);

	/* Peak gust speed added on top of the base wind, cm/s. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Wind")
	void SetGusting(float NewGustAmplitudeCmS, float NewGustFrequencyHz);

	/**
	 * Wind velocity at a world position, cm/s.
	 * Deterministic in (position, time): every machine sampling the same inputs agrees.
	 */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Wind")
	FVector SampleWind(const FVector& WorldPosition) const;

	/**
	 * Aerodynamic force from the wind on a body, in Unreal force units.
	 *
	 * @param VelocityCmS  The body's own velocity; drag acts on airspeed, not ground speed, which is
	 *                     what makes a drone drift downwind while apparently holding still.
	 * @param DragArea     Reference area, m^2.
	 */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Wind")
	FVector ComputeWindForce(const FVector& WorldPosition, const FVector& VelocityCmS, float DragCoefficient, float DragArea, float AirDensity = 1.225f) const;

protected:

	UPROPERTY(EditAnywhere, Category = "ForgeDrone|Wind")
	FVector BaseWindCmS = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "ForgeDrone|Wind", meta = (ClampMin = "0.0"))
	float GustAmplitudeCmS = 150.f;

	UPROPERTY(EditAnywhere, Category = "ForgeDrone|Wind", meta = (ClampMin = "0.0"))
	float GustFrequencyHz = 0.15f;

	/* Spatial scale of the gust field, centimetres. Larger means broader, slower-moving cells. */
	UPROPERTY(EditAnywhere, Category = "ForgeDrone|Wind", meta = (ClampMin = "1.0"))
	float TurbulenceScaleCm = 4000.f;
};
