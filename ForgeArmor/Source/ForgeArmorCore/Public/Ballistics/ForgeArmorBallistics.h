// Copyright Impact-Forge. Pure armor penetration math - no UObjects, unit-testable.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "ForgeArmorTypes.h"

/**
 * ForgeArmor::Ballistics
 *
 * Real-world-anchored armor penetration model in the War Thunder / Gunner HEAT PC
 * tradition: every ammunition nature is authored with a reference penetration datum
 * (mm RHA at a reference speed, 0 deg obliquity) and every plate with a material and
 * a real thickness in mm. The solver produces perforation / partial / ricochet
 * outcomes with residual velocity.
 *
 * Units: mm for armor and penetration, m/s for speeds, J for energy, kg for mass,
 * degrees for angles. Angles are measured FROM THE PLATE NORMAL (0 = head on,
 * 90 = fully grazing).
 *
 * Everything here is deterministic given its inputs; probabilistic decisions
 * (ricochet band, fire ignition) take an externally supplied 0..1 roll so the
 * functions stay unit-testable.
 */
namespace ForgeArmor::Ballistics
{
	// ---------------------------------------------------------------- KE solver

	/** Kinetic penetrator description (resolved from FForgeAmmoBallisticSpec). */
	struct FKEPenetrator
	{
		EForgePenetratorClass PenetratorClass = EForgePenetratorClass::AP;
		/** Flat RHA penetration (mm) at ReferenceSpeedMS. */
		double PenetrationRefMM = 5.0;
		/** Speed the datum is quoted at (m/s). */
		double ReferenceSpeedMS = 800.0;
		double CaliberMM = 7.62;
		double MassKG = 0.01;
		/** <= 0 uses the class default DeMarre exponent. */
		double ExponentOverride = 0.0;
	};

	/** Effective plate description (material factors already resolved). */
	struct FPlate
	{
		double ThicknessMM = 10.0;
		/** RHA-equivalence multiplier vs KE (RHA = 1.0). */
		double KFactorKE = 1.0;
		/** RHA-equivalence multiplier vs CE (RHA = 1.0). */
		double KFactorCE = 1.0;
		/** 0..1 spall suppression behind this plate. */
		double SpallSuppression = 0.0;
	};

	enum class EKEOutcome : uint8
	{
		Stopped,
		Penetrated,
		Ricochet
	};

	struct FKEResult
	{
		EKEOutcome Outcome = EKEOutcome::Stopped;
		/** Penetration available at impact speed (mm RHA, before slope effects). */
		double PenAvailableMM = 0.0;
		/** Effective plate thickness the round had to defeat (mm RHA). */
		double EffectiveThicknessMM = 0.0;
		/** Geometric line-of-sight path through the plate (mm). */
		double LineOfSightMM = 0.0;
		/** Exit speed when Penetrated, deflected speed when Ricochet (m/s). */
		double ResidualSpeedMS = 0.0;
		/** Energy imparted to the plate (J). */
		double EnergyImpartedJ = 0.0;
		/** PenAvailable / EffectiveThickness when Stopped (how close it came). */
		double PartialPenetrationFraction = 0.0;
		/** Near-perforation (>= 85% of Teff): the back face sheds some spall. */
		bool bBackFaceSpall = false;
	};

	/** Class default DeMarre velocity exponent. */
	FORGEARMORCORE_API double DeMarreExponent(EForgePenetratorClass PenetratorClass);

	/** Penetration available at a given speed: PenRef * (V / VRef)^n. */
	FORGEARMORCORE_API double PenetrationAtSpeed(EForgePenetratorClass PenetratorClass, double PenetrationRefMM, double ReferenceSpeedMS, double SpeedMS, double ExponentOverride = 0.0);

	/** Slope-effect multiplier (>= 1) applied to LOS thickness, per penetrator class. */
	FORGEARMORCORE_API double SlopeModifier(EForgePenetratorClass PenetratorClass, double AngleFromNormalDeg);

	/** Overmatch: calibers much larger than the plate defeat it more easily. clamp((D/t)^0.25, 0.85, 1.25). */
	FORGEARMORCORE_API double OvermatchFactor(double CaliberMM, double PlateThicknessMM);

	/** Geometric line of sight through the plate: t / cos(angle), capped near grazing. */
	FORGEARMORCORE_API double LineOfSightThickness(double ThicknessMM, double AngleFromNormalDeg);

	/** Full effective thickness in mm RHA: LOS * material * slope / overmatch. */
	FORGEARMORCORE_API double EffectiveThickness(const FPlate& Plate, EForgePenetratorClass PenetratorClass, double AngleFromNormalDeg, double CaliberMM);

	/** Critical ricochet angle from normal for the class, shifted by overmatch dig-in. */
	FORGEARMORCORE_API double RicochetCriticalAngleDeg(EForgePenetratorClass PenetratorClass, double CaliberMM, double PlateThicknessMM);

	/** 0..1 ricochet probability given impact angle vs the critical angle (+-band ramp). */
	FORGEARMORCORE_API double RicochetProbability(double AngleFromNormalDeg, double CriticalAngleDeg);

	/** Fraction of impact speed retained by a deflected round (grazing keeps more). */
	FORGEARMORCORE_API double RicochetSpeedRetention(double AngleFromNormalDeg, double CriticalAngleDeg);

	/**
	 * Solve one kinetic impact against one plate.
	 * @param RicochetRoll01 externally supplied uniform roll deciding the ricochet band.
	 */
	FORGEARMORCORE_API FKEResult SolveKEImpact(const FKEPenetrator& Ammo, const FPlate& Plate, double ImpactSpeedMS, double AngleFromNormalDeg, double RicochetRoll01);

	// ---------------------------------------------------------------- CE solver

	/** One element of a shaped-charge path (plate, air gap or ERA tile). */
	struct FCEPathElement
	{
		double ThicknessMM = 0.0;
		double KFactorCE = 1.0;
		bool bIntactERA = false;
		double ERACutCEMM = 0.0;
	};

	/** Standoff degradation for a jet formed beyond its optimal standoff (mm). */
	FORGEARMORCORE_API double CEStandoffFactor(double StandoffMM);

	/**
	 * Consume a shaped-charge jet through an armor array.
	 * @return residual penetration in mm RHA (<= 0: defeated inside the array).
	 * @param bOutERAConsumed set if an intact ERA element was spent.
	 */
	FORGEARMORCORE_API double SolveCEResidualMM(double JetPenetrationMM, TArrayView<const FCEPathElement> Path, double StandoffMM, bool& bOutERAConsumed);

	// ---------------------------------------------------------------- Spall

	struct FSpallCone
	{
		double HalfAngleDeg = 20.0;
		int32 FragmentCount = 0;
		double TotalEnergyJ = 0.0;
	};

	/**
	 * Behind-armor spall for a perforating KE hit.
	 * Marginal perforations (Teff close to Pavail) shed wide, slow spall; clean
	 * overmatch produces a tight energetic cone.
	 */
	FORGEARMORCORE_API FSpallCone ComputeSpall(double EffectiveThicknessMM, double PenAvailableMM, double LineOfSightMM, double EnergyImpartedJ, double SpallSuppression);

	/** Residual shaped-charge jet modelled as a narrow spall cone. */
	FORGEARMORCORE_API FSpallCone ComputeJetCone(double ResidualPenetrationMM, double JetPenetrationMM);

	/** HESH scab: wide cone, mass scaled fragments - only against susceptible arrays. */
	FORGEARMORCORE_API bool HESHProducesScab(double PlateThicknessMM, double CaliberMM, double HESHFactor);
	FORGEARMORCORE_API FSpallCone ComputeHESHScab(double CaliberMM, double PlateThicknessMM);

	// ---------------------------------------------------------------- HE / blast

	/** Armor defeated by contact blast: ~25 * (TNT kg)^(1/3) mm RHA. */
	FORGEARMORCORE_API double BlastPenetrationMM(double TNTEquivalentKG);

	/** Blast energy (J) from a TNT-equivalent mass (4.184 MJ/kg). */
	FORGEARMORCORE_API double BlastEnergyJ(double TNTEquivalentKG);

	// ---------------------------------------------------------------- Utilities

	FORGEARMORCORE_API double KineticEnergyJ(double MassKG, double SpeedMS);
	FORGEARMORCORE_API double SpeedFromKineticEnergy(double EnergyJ, double MassKG);

	/**
	 * Rough DeMarre estimate of flat RHA penetration for ammunition without an
	 * authored spec, from bullet mass/caliber/speed. Ball (FMJ) natures are scaled
	 * down heavily. Authored specs should always be preferred.
	 */
	FORGEARMORCORE_API double EstimatePenetrationRefMM(EForgePenetratorClass PenetratorClass, double MassKG, double CaliberMM, double ReferenceSpeedMS);

	/**
	 * Approximate solid angle (sr) subtended by a box of the given half extents (cm)
	 * whose center sits DistanceCm away. Projected-area approximation, clamped to 2*pi.
	 * Used by the expected-hit interior damage model.
	 */
	FORGEARMORCORE_API double ApproxSolidAngleSr(const FVector& HalfExtentCm, double DistanceCm);

	/** Solid angle of a cone with the given half angle (sr). */
	FORGEARMORCORE_API double ConeSolidAngleSr(double HalfAngleDeg);
}
