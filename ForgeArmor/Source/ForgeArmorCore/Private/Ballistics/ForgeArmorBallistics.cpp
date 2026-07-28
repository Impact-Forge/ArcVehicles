// Copyright Impact-Forge. Pure armor penetration math implementation.

#include "Ballistics/ForgeArmorBallistics.h"

namespace ForgeArmor::Ballistics
{
	namespace
	{
		// Slope-effect table sample angles (deg from plate normal).
		constexpr double SlopeAngles[5] = { 0.0, 30.0, 45.0, 60.0, 70.0 };

		// Per-class slope-effect multipliers at the sample angles. Values follow the
		// familiar pattern: blunt/capped rounds bite into slopes, cored rounds shatter,
		// long rods barely care beyond raw LOS.
		struct FSlopeRow
		{
			double Values[5];
		};

		const FSlopeRow& SlopeRowFor(EForgePenetratorClass PenetratorClass)
		{
			static const FSlopeRow RowAP     = { { 1.00, 1.05, 1.15, 1.30, 1.45 } };
			static const FSlopeRow RowAPCBC  = { { 1.00, 1.00, 1.05, 1.15, 1.25 } };
			static const FSlopeRow RowAPCR   = { { 1.00, 1.10, 1.25, 1.45, 1.70 } };
			static const FSlopeRow RowAPDS   = { { 1.00, 1.05, 1.10, 1.20, 1.30 } };
			static const FSlopeRow RowAPFSDS = { { 1.00, 1.00, 1.00, 1.05, 1.10 } };

			switch (PenetratorClass)
			{
			case EForgePenetratorClass::APCBC:  return RowAPCBC;
			case EForgePenetratorClass::APCR:   return RowAPCR;
			case EForgePenetratorClass::APDS:   return RowAPDS;
			case EForgePenetratorClass::APFSDS: return RowAPFSDS;
			case EForgePenetratorClass::FMJ:
			case EForgePenetratorClass::AP:
			case EForgePenetratorClass::API:
			default:                            return RowAP;
			}
		}

		double BaseRicochetCriticalAngle(EForgePenetratorClass PenetratorClass)
		{
			switch (PenetratorClass)
			{
			case EForgePenetratorClass::FMJ:    return 60.0;
			case EForgePenetratorClass::AP:     return 64.0;
			case EForgePenetratorClass::API:    return 64.0;
			case EForgePenetratorClass::APCBC:  return 70.0;
			case EForgePenetratorClass::APCR:   return 62.0;
			case EForgePenetratorClass::APDS:   return 72.0;
			case EForgePenetratorClass::APFSDS: return 81.0;
			default:                            return 90.0; // CE natures never ricochet here
			}
		}

		// Width of the probabilistic ricochet band around the critical angle (deg).
		constexpr double RicochetBandHalfWidthDeg = 4.0;

		// Grazing cap: beyond this obliquity LOS growth is capped to avoid infinities.
		constexpr double MaxLOSAngleDeg = 85.0;
	}

	double DeMarreExponent(EForgePenetratorClass PenetratorClass)
	{
		switch (PenetratorClass)
		{
		case EForgePenetratorClass::FMJ:    return 1.43;
		case EForgePenetratorClass::AP:     return 1.43;
		case EForgePenetratorClass::API:    return 1.43;
		case EForgePenetratorClass::APCBC:  return 1.43;
		case EForgePenetratorClass::APCR:   return 1.50;
		case EForgePenetratorClass::APDS:   return 1.40;
		case EForgePenetratorClass::APFSDS: return 1.15; // quasi-linear long-rod regime
		default:                            return 1.43;
		}
	}

	double PenetrationAtSpeed(EForgePenetratorClass PenetratorClass, double PenetrationRefMM, double ReferenceSpeedMS, double SpeedMS, double ExponentOverride)
	{
		if (PenetrationRefMM <= 0.0 || ReferenceSpeedMS <= 0.0 || SpeedMS <= 0.0)
		{
			return 0.0;
		}
		const double Exponent = ExponentOverride > 0.0 ? ExponentOverride : DeMarreExponent(PenetratorClass);
		return PenetrationRefMM * FMath::Pow(SpeedMS / ReferenceSpeedMS, Exponent);
	}

	double SlopeModifier(EForgePenetratorClass PenetratorClass, double AngleFromNormalDeg)
	{
		const FSlopeRow& Row = SlopeRowFor(PenetratorClass);
		const double Angle = FMath::Clamp(AngleFromNormalDeg, 0.0, 90.0);

		if (Angle <= SlopeAngles[0])
		{
			return Row.Values[0];
		}
		for (int32 i = 1; i < 5; ++i)
		{
			if (Angle <= SlopeAngles[i])
			{
				const double T = (Angle - SlopeAngles[i - 1]) / (SlopeAngles[i] - SlopeAngles[i - 1]);
				return FMath::Lerp(Row.Values[i - 1], Row.Values[i], T);
			}
		}
		// Beyond the last sample: continue the final segment's slope up to 85 deg.
		const double LastSlopePerDeg = (Row.Values[4] - Row.Values[3]) / (SlopeAngles[4] - SlopeAngles[3]);
		return Row.Values[4] + LastSlopePerDeg * (FMath::Min(Angle, MaxLOSAngleDeg) - SlopeAngles[4]);
	}

	double OvermatchFactor(double CaliberMM, double PlateThicknessMM)
	{
		if (CaliberMM <= 0.0 || PlateThicknessMM <= 0.0)
		{
			return 1.0;
		}
		return FMath::Clamp(FMath::Pow(CaliberMM / PlateThicknessMM, 0.25), 0.85, 1.25);
	}

	double LineOfSightThickness(double ThicknessMM, double AngleFromNormalDeg)
	{
		const double Angle = FMath::Clamp(AngleFromNormalDeg, 0.0, MaxLOSAngleDeg);
		return ThicknessMM / FMath::Cos(FMath::DegreesToRadians(Angle));
	}

	double EffectiveThickness(const FPlate& Plate, EForgePenetratorClass PenetratorClass, double AngleFromNormalDeg, double CaliberMM)
	{
		const double LOS = LineOfSightThickness(Plate.ThicknessMM, AngleFromNormalDeg);
		const double Slope = SlopeModifier(PenetratorClass, AngleFromNormalDeg);
		const double Overmatch = OvermatchFactor(CaliberMM, Plate.ThicknessMM);
		return LOS * Plate.KFactorKE * Slope / Overmatch;
	}

	double RicochetCriticalAngleDeg(EForgePenetratorClass PenetratorClass, double CaliberMM, double PlateThicknessMM)
	{
		double Critical = BaseRicochetCriticalAngle(PenetratorClass);
		if (CaliberMM > 0.0 && PlateThicknessMM > 0.0)
		{
			// Heavy overmatch digs in: shift the critical angle towards grazing.
			const double OvermatchShift = 10.0 * FMath::Clamp(CaliberMM / (2.0 * PlateThicknessMM) - 0.5, 0.0, 1.0);
			Critical += OvermatchShift;
		}
		return FMath::Min(Critical, 89.0);
	}

	double RicochetProbability(double AngleFromNormalDeg, double CriticalAngleDeg)
	{
		if (CriticalAngleDeg >= 89.0)
		{
			return 0.0;
		}
		const double Delta = AngleFromNormalDeg - CriticalAngleDeg;
		if (Delta <= -RicochetBandHalfWidthDeg)
		{
			return 0.0;
		}
		if (Delta >= RicochetBandHalfWidthDeg)
		{
			return 1.0;
		}
		return (Delta + RicochetBandHalfWidthDeg) / (2.0 * RicochetBandHalfWidthDeg);
	}

	double RicochetSpeedRetention(double AngleFromNormalDeg, double CriticalAngleDeg)
	{
		const double Range = FMath::Max(90.0 - CriticalAngleDeg, 1.0);
		const double T = FMath::Clamp((AngleFromNormalDeg - CriticalAngleDeg) / Range, 0.0, 1.0);
		return 0.55 + 0.40 * T;
	}

	FKEResult SolveKEImpact(const FKEPenetrator& Ammo, const FPlate& Plate, double ImpactSpeedMS, double AngleFromNormalDeg, double RicochetRoll01)
	{
		FKEResult Result;
		Result.LineOfSightMM = LineOfSightThickness(Plate.ThicknessMM, AngleFromNormalDeg);
		Result.PenAvailableMM = PenetrationAtSpeed(Ammo.PenetratorClass, Ammo.PenetrationRefMM, Ammo.ReferenceSpeedMS, ImpactSpeedMS, Ammo.ExponentOverride);
		Result.EffectiveThicknessMM = EffectiveThickness(Plate, Ammo.PenetratorClass, AngleFromNormalDeg, Ammo.CaliberMM);

		const double ImpactEnergyJ = KineticEnergyJ(Ammo.MassKG, ImpactSpeedMS);

		// Ricochet check first: shallow impacts glance off before any penetration solve.
		const double CriticalAngle = RicochetCriticalAngleDeg(Ammo.PenetratorClass, Ammo.CaliberMM, Plate.ThicknessMM);
		const double Probability = RicochetProbability(AngleFromNormalDeg, CriticalAngle);
		if (Probability > 0.0 && RicochetRoll01 <= Probability)
		{
			Result.Outcome = EKEOutcome::Ricochet;
			Result.ResidualSpeedMS = ImpactSpeedMS * RicochetSpeedRetention(AngleFromNormalDeg, CriticalAngle);
			Result.EnergyImpartedJ = ImpactEnergyJ - KineticEnergyJ(Ammo.MassKG, Result.ResidualSpeedMS);
			return Result;
		}

		if (Result.PenAvailableMM > Result.EffectiveThicknessMM)
		{
			Result.Outcome = EKEOutcome::Penetrated;
			// Invert the DeMarre relation on the remaining penetration budget.
			const double Exponent = Ammo.ExponentOverride > 0.0 ? Ammo.ExponentOverride : DeMarreExponent(Ammo.PenetratorClass);
			const double ResidualFraction = (Result.PenAvailableMM - Result.EffectiveThicknessMM) / Ammo.PenetrationRefMM;
			Result.ResidualSpeedMS = FMath::Min(Ammo.ReferenceSpeedMS * FMath::Pow(ResidualFraction, 1.0 / Exponent), ImpactSpeedMS);
			Result.EnergyImpartedJ = ImpactEnergyJ - KineticEnergyJ(Ammo.MassKG, Result.ResidualSpeedMS);
		}
		else
		{
			Result.Outcome = EKEOutcome::Stopped;
			Result.PartialPenetrationFraction = Result.EffectiveThicknessMM > 0.0
				? FMath::Clamp(Result.PenAvailableMM / Result.EffectiveThicknessMM, 0.0, 1.0)
				: 0.0;
			Result.EnergyImpartedJ = ImpactEnergyJ;
			Result.bBackFaceSpall = Result.PartialPenetrationFraction >= 0.85;
		}
		return Result;
	}

	double CEStandoffFactor(double StandoffMM)
	{
		if (StandoffMM <= 400.0)
		{
			return 1.0;
		}
		return FMath::Clamp(1.0 - 0.2 * (StandoffMM - 400.0) / 500.0, 0.4, 1.0);
	}

	double SolveCEResidualMM(double JetPenetrationMM, TArrayView<const FCEPathElement> Path, double StandoffMM, bool& bOutERAConsumed)
	{
		bOutERAConsumed = false;
		double Residual = JetPenetrationMM * CEStandoffFactor(StandoffMM);
		for (const FCEPathElement& Element : Path)
		{
			if (Residual <= 0.0)
			{
				return 0.0;
			}
			Residual -= Element.ThicknessMM * Element.KFactorCE;
			if (Element.bIntactERA)
			{
				Residual -= Element.ERACutCEMM;
				bOutERAConsumed = true;
			}
		}
		return FMath::Max(Residual, 0.0);
	}

	FSpallCone ComputeSpall(double EffectiveThicknessMM, double PenAvailableMM, double LineOfSightMM, double EnergyImpartedJ, double SpallSuppression)
	{
		FSpallCone Cone;
		const double Ratio = PenAvailableMM > 0.0 ? EffectiveThicknessMM / PenAvailableMM : 1.0;
		Cone.HalfAngleDeg = FMath::Clamp(10.0 + 25.0 * Ratio, 12.0, 40.0);

		const double Suppression = FMath::Clamp(1.0 - SpallSuppression, 0.0, 1.0);
		Cone.FragmentCount = FMath::RoundToInt32(FMath::Clamp(6.0 + 0.35 * LineOfSightMM, 4.0, 48.0) * Suppression);
		Cone.TotalEnergyJ = 0.10 * FMath::Max(EnergyImpartedJ, 0.0) * Suppression;
		return Cone;
	}

	FSpallCone ComputeJetCone(double ResidualPenetrationMM, double JetPenetrationMM)
	{
		FSpallCone Cone;
		Cone.HalfAngleDeg = 5.0;
		const double Scale = JetPenetrationMM > 0.0 ? FMath::Clamp(ResidualPenetrationMM / JetPenetrationMM, 0.0, 1.0) : 0.0;
		// A copper jet carries on the order of hundreds of kJ; scale by remaining length.
		Cone.TotalEnergyJ = 250000.0 * Scale;
		Cone.FragmentCount = FMath::Max(FMath::RoundToInt32(12.0 * Scale), 1);
		return Cone;
	}

	bool HESHProducesScab(double PlateThicknessMM, double CaliberMM, double HESHFactor)
	{
		if (HESHFactor < 0.5)
		{
			// Spaced/lined/composite arrays decouple the shock wave.
			return false;
		}
		return PlateThicknessMM < 1.3 * CaliberMM;
	}

	FSpallCone ComputeHESHScab(double CaliberMM, double PlateThicknessMM)
	{
		FSpallCone Cone;
		Cone.HalfAngleDeg = 40.0;
		Cone.FragmentCount = FMath::RoundToInt32(FMath::Clamp(3.0 * (6.0 + 0.35 * PlateThicknessMM), 12.0, 96.0));
		// Scab energy scales with charge mass ~ caliber^3.
		Cone.TotalEnergyJ = 40.0 * CaliberMM * CaliberMM * CaliberMM * 1e-3;
		return Cone;
	}

	double BlastPenetrationMM(double TNTEquivalentKG)
	{
		if (TNTEquivalentKG <= 0.0)
		{
			return 0.0;
		}
		return 25.0 * FMath::Pow(TNTEquivalentKG, 1.0 / 3.0);
	}

	double BlastEnergyJ(double TNTEquivalentKG)
	{
		return 4.184e6 * FMath::Max(TNTEquivalentKG, 0.0);
	}

	double KineticEnergyJ(double MassKG, double SpeedMS)
	{
		return 0.5 * FMath::Max(MassKG, 0.0) * SpeedMS * SpeedMS;
	}

	double SpeedFromKineticEnergy(double EnergyJ, double MassKG)
	{
		if (MassKG <= 0.0 || EnergyJ <= 0.0)
		{
			return 0.0;
		}
		return FMath::Sqrt(2.0 * EnergyJ / MassKG);
	}

	double EstimatePenetrationRefMM(EForgePenetratorClass PenetratorClass, double MassKG, double CaliberMM, double ReferenceSpeedMS)
	{
		if (MassKG <= 0.0 || CaliberMM <= 0.0 || ReferenceSpeedMS <= 0.0)
		{
			return 0.0;
		}
		// DeMarre-style estimate calibrated so 12.7mm AP (45.9 g @ 850 m/s) ~ 25 mm.
		constexpr double Calibration = 0.1144;
		double Estimate = Calibration * FMath::Sqrt(MassKG) * FMath::Pow(ReferenceSpeedMS, 1.43) / FMath::Pow(CaliberMM, 1.07);
		if (PenetratorClass == EForgePenetratorClass::FMJ)
		{
			Estimate *= 0.35; // soft cores smear instead of penetrating
		}
		return Estimate;
	}

	double ApproxSolidAngleSr(const FVector& HalfExtentCm, double DistanceCm)
	{
		if (DistanceCm <= 1.0)
		{
			return 2.0 * UE_DOUBLE_PI;
		}
		// Mean projected area of a box over orientations: half its surface area / 2.
		const double SurfaceArea = 8.0 * (HalfExtentCm.X * HalfExtentCm.Y + HalfExtentCm.Y * HalfExtentCm.Z + HalfExtentCm.Z * HalfExtentCm.X);
		const double MeanProjectedArea = SurfaceArea / 4.0;
		return FMath::Min(MeanProjectedArea / (DistanceCm * DistanceCm), 2.0 * UE_DOUBLE_PI);
	}

	double ConeSolidAngleSr(double HalfAngleDeg)
	{
		const double HalfAngleRad = FMath::DegreesToRadians(FMath::Clamp(HalfAngleDeg, 0.0, 90.0));
		return 2.0 * UE_DOUBLE_PI * (1.0 - FMath::Cos(HalfAngleRad));
	}
}
