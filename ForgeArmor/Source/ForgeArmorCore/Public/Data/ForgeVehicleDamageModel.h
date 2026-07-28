// Copyright Impact-Forge. Interior damage model of one vehicle: modules, crew stations, fire graph.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ForgeArmorTypes.h"

#include "ForgeVehicleDamageModel.generated.h"

/**
 * The functional interior of a vehicle: every module that can be damaged
 * (engine, ammo, fuel, crew stations, running gear) with volumes for the
 * behind-armor expected-hit model and the rules tying module states to
 * vehicle-level kill states.
 */
UCLASS(BlueprintType)
class FORGEARMORCORE_API UForgeVehicleDamageModel : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modules", meta = (TitleProperty = "ModuleId"))
	TArray<FForgeDamageModuleDef> Modules;

	/** Aggregate structural hitpoints; reaching zero destroys the vehicle outright. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (ClampMin = "1"))
	float StructuralMaxHP = 1000.f;

	/** Open-topped: HE overpressure and strafing reach the crew directly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	bool bOpenTop = false;

	/** Blow-out ammo panels demote a catastrophic ammo detonation to a survivable one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	bool bBlowOutPanels = false;

	/** Fires spread between modules whose volumes sit within this range (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Fire", meta = (ClampMin = "0"))
	float FireSpreadRadiusCm = 150.f;

	/** How many wheels (per side, tracks excluded) may be lost before mobility is killed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (ClampMin = "0"))
	int32 WheelLossMobilityKillCount = 2;

	const FForgeDamageModuleDef* FindModule(const FName& ModuleId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
