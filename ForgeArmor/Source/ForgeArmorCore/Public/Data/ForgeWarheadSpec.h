// Copyright Impact-Forge. Chemical-energy warhead data (HEAT / HESH / HE).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ForgeArmorTypes.h"

#include "ForgeWarheadSpec.generated.h"

/**
 * A chemical-energy warhead: shaped charge, squash head or blast.
 * Referenced by game-side projectile/explosion actors when calling the
 * registry evaluators (EvaluateCEImpact / EvaluateHESHImpact / EvaluateBlast).
 */
UCLASS(BlueprintType)
class FORGEARMORCORE_API UForgeWarheadSpec : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** HEAT, HESH or HE. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warhead")
	EForgePenetratorClass WarheadClass = EForgePenetratorClass::HEAT;

	/** Shaped-charge jet penetration, mm RHA at optimal standoff (HEAT). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warhead", meta = (EditCondition = "WarheadClass == EForgePenetratorClass::HEAT", ClampMin = "0"))
	float JetPenetrationMM = 400.f;

	/** Warhead caliber in mm (HESH scab rule, FX scale). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warhead", meta = (ClampMin = "1"))
	float CaliberMM = 85.f;

	/** Explosive filler as kg of TNT equivalent (HE overpressure, APHE-style bursts). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warhead", meta = (ClampMin = "0"))
	float TNTEquivalentKG = 1.0f;

	/** Overpressure damage radius against open-topped / unarmored targets (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warhead", meta = (ClampMin = "0"))
	float BlastRadiusCm = 600.f;
};
