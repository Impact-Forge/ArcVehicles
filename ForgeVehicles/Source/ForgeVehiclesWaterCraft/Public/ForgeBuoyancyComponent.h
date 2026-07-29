// Copyright Impact-Forge. Water-craft physics authored for Forge Vehicles.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForgeBuoyancyComponent.generated.h"

class UPrimitiveComponent;

/**
 * Multi-pontoon buoyancy solver for Forge water craft.
 *
 * Follows the same force-based, tunable-per-property design language as the RTune and K2 physics
 * modules: it samples a set of relative pontoon points against a water surface height and applies an
 * upward buoyancy force at each submerged point, plus configurable linear/angular water damping, to
 * the target simulating primitive (by default the owning vehicle's root).
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESWATERCRAFT_API UForgeBuoyancyComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeBuoyancyComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// End UActorComponent interface

	/* Sets the world-space Z height of the water surface (e.g. driven by an ocean/water plane). */
	UFUNCTION(BlueprintCallable, Category = "Buoyancy")
	void SetWaterHeight(float NewWaterHeight) { WaterHeight = NewWaterHeight; }

	/* Returns how submerged the hull currently is, 0 (dry) .. 1 (all pontoons fully under). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Buoyancy")
	float GetSubmersionRatio() const { return LastSubmersionRatio; }

	/* Optional named primitive to float; when empty the owner's root primitive is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy")
	FName TargetComponentName;

	/* Relative pontoon sample points (in the hull's local space). Typically the four corners + centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy")
	TArray<FVector> Pontoons;

	/* Upward force applied at a fully-submerged pontoon (uu * kg / s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy", meta = (ClampMin = "0.0"))
	float BuoyancyForcePerPontoon = 12000.f;

	/* Depth (cm) over which a pontoon ramps from no buoyancy to full buoyancy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy", meta = (ClampMin = "1.0"))
	float PontoonRadius = 60.f;

	/* World Z of the water surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy")
	float WaterHeight = 0.f;

	/* Linear velocity damping applied to the body while it is in the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy", meta = (ClampMin = "0.0"))
	float WaterLinearDamping = 1.5f;

	/* Angular velocity damping applied to the body while it is in the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy", meta = (ClampMin = "0.0"))
	float WaterAngularDamping = 2.5f;

	/* Draw the pontoon samples and their submersion for debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buoyancy|Debug")
	bool bShowDebug = false;

protected:

	/* Resolves and caches the primitive that will be floated. */
	void ResolveTargetPrimitive();

	UPROPERTY(Transient)
	UPrimitiveComponent* TargetPrimitive = nullptr;

	float LastSubmersionRatio = 0.f;
};
