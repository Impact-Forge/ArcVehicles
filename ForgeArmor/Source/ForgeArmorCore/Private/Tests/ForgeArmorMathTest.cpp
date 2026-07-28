// Copyright Impact-Forge. Numeric self-tests for the armor penetration math.

#include "Misc/AutomationTest.h"

#include "Ballistics/ForgeArmorBallistics.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace ForgeArmor::Ballistics;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForgeArmorKEMathTest, "Forge.Armor.Math.KE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FForgeArmorKEMathTest::RunTest(const FString& Parameters)
{
	// ---- Worked example A: M2 .50 AP (25mm @ 850 m/s datum) vs BTR-style side.
	// 7mm high-hardness plate (K 1.15) at 30 deg, impact 800 m/s.
	FKEPenetrator FiftyAP;
	FiftyAP.PenetratorClass = EForgePenetratorClass::AP;
	FiftyAP.PenetrationRefMM = 25.0;
	FiftyAP.ReferenceSpeedMS = 850.0;
	FiftyAP.CaliberMM = 12.7;
	FiftyAP.MassKG = 0.0459;

	FPlate BTRSide;
	BTRSide.ThicknessMM = 7.0;
	BTRSide.KFactorKE = 1.15;

	{
		const FKEResult Result = SolveKEImpact(FiftyAP, BTRSide, 800.0, 30.0, /*RicochetRoll*/ 0.99);
		TestTrue(TEXT(".50 AP vs 7mm HHS @30deg perforates"), Result.Outcome == EKEOutcome::Penetrated);
		TestTrue(TEXT(".50 AP penetration available ~22.9mm"), FMath::IsNearlyEqual(Result.PenAvailableMM, 22.92, 0.15));
		TestTrue(TEXT(".50 AP effective thickness ~8.4mm"), FMath::IsNearlyEqual(Result.EffectiveThicknessMM, 8.41, 0.15));
		TestTrue(TEXT(".50 AP residual speed ~581 m/s"), Result.ResidualSpeedMS > 570.0 && Result.ResidualSpeedMS < 592.0);
		TestTrue(TEXT(".50 AP imparted energy ~6.9 kJ"), Result.EnergyImpartedJ > 6500.0 && Result.EnergyImpartedJ < 7400.0);

		const FSpallCone Spall = ComputeSpall(Result.EffectiveThicknessMM, Result.PenAvailableMM, Result.LineOfSightMM, Result.EnergyImpartedJ, 0.0);
		TestTrue(TEXT(".50 AP spall count in [4, 12]"), Spall.FragmentCount >= 4 && Spall.FragmentCount <= 12);
		TestTrue(TEXT(".50 AP spall cone 12-40 deg"), Spall.HalfAngleDeg >= 12.0 && Spall.HalfAngleDeg <= 40.0);
	}

	// Same plate near-grazing: must deflect regardless of roll inside the sure band.
	{
		const FKEResult Result = SolveKEImpact(FiftyAP, BTRSide, 800.0, 75.0, 0.5);
		TestTrue(TEXT(".50 AP vs 7mm @75deg ricochets"), Result.Outcome == EKEOutcome::Ricochet);
		TestTrue(TEXT("Ricochet keeps most of its speed"), Result.ResidualSpeedMS > 0.55 * 800.0 && Result.ResidualSpeedMS < 800.0);
	}

	// ---- Worked example B: 30mm APDS (60mm @ 1120 m/s) vs composite MBT turret front.
	FKEPenetrator ThirtyAPDS;
	ThirtyAPDS.PenetratorClass = EForgePenetratorClass::APDS;
	ThirtyAPDS.PenetrationRefMM = 60.0;
	ThirtyAPDS.ReferenceSpeedMS = 1120.0;
	ThirtyAPDS.CaliberMM = 30.0;
	ThirtyAPDS.MassKG = 0.3;

	FPlate TurretFront;
	TurretFront.ThicknessMM = 500.0;
	TurretFront.KFactorKE = 1.25;

	{
		const FKEResult Result = SolveKEImpact(ThirtyAPDS, TurretFront, 970.0, 0.0, 0.99);
		TestTrue(TEXT("30mm APDS vs 500mm composite is stopped"), Result.Outcome == EKEOutcome::Stopped);
		TestTrue(TEXT("30mm APDS pen available ~49mm"), FMath::IsNearlyEqual(Result.PenAvailableMM, 49.06, 0.4));
		TestTrue(TEXT("30mm APDS residual is zero"), Result.ResidualSpeedMS == 0.0);
		TestTrue(TEXT("30mm APDS barely scratches (<10%)"), Result.PartialPenetrationFraction < 0.10);
		TestTrue(TEXT("No back-face spall from a shallow stop"), !Result.bBackFaceSpall);
	}

	// ---- DeMarre datum sanity: at the reference speed the datum is returned exactly.
	TestTrue(TEXT("Pen at reference speed equals datum"),
		FMath::IsNearlyEqual(PenetrationAtSpeed(EForgePenetratorClass::AP, 100.0, 900.0, 900.0), 100.0, 1e-6));

	// Monotonic in speed.
	TestTrue(TEXT("Pen grows with speed"),
		PenetrationAtSpeed(EForgePenetratorClass::AP, 100.0, 900.0, 1000.0) >
		PenetrationAtSpeed(EForgePenetratorClass::AP, 100.0, 900.0, 800.0));

	// APFSDS is nearly slope-insensitive, APCR is punished on slopes.
	TestTrue(TEXT("APFSDS slope modifier @60 is small"), SlopeModifier(EForgePenetratorClass::APFSDS, 60.0) < 1.10);
	TestTrue(TEXT("APCR slope modifier @60 is severe"), SlopeModifier(EForgePenetratorClass::APCR, 60.0) > 1.40);

	// LOS at 60 degrees doubles the path.
	TestTrue(TEXT("LOS @60deg = 2x"), FMath::IsNearlyEqual(LineOfSightThickness(80.0, 60.0), 160.0, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForgeArmorCEMathTest, "Forge.Armor.Math.CE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FForgeArmorCEMathTest::RunTest(const FString& Parameters)
{
	// ---- Worked example C: RPG-7 PG-7VL class jet, 500mm RHA.
	constexpr double JetPenMM = 500.0;

	// (i) 80mm RHA + spall liner (+20mm CE): residual 400mm.
	{
		const FCEPathElement Path[] = {
			{ 80.0, 1.0, false, 0.0 },
			{ 20.0, 1.0, false, 0.0 },
		};
		bool bERAConsumed = false;
		const double Residual = SolveCEResidualMM(JetPenMM, Path, 0.0, bERAConsumed);
		TestTrue(TEXT("PG-7VL vs 80mm+liner residual ~400mm"), FMath::IsNearlyEqual(Residual, 400.0, 5.0));
		TestFalse(TEXT("No ERA consumed"), bERAConsumed);
	}

	// (ii) with an intact ERA tile (CE cut 300mm): residual 100mm, tile spent.
	{
		const FCEPathElement Path[] = {
			{ 0.0, 1.0, true, 300.0 },
			{ 80.0, 1.0, false, 0.0 },
			{ 20.0, 1.0, false, 0.0 },
		};
		bool bERAConsumed = false;
		const double Residual = SolveCEResidualMM(JetPenMM, Path, 0.0, bERAConsumed);
		TestTrue(TEXT("PG-7VL vs ERA residual ~100mm"), FMath::IsNearlyEqual(Residual, 100.0, 5.0));
		TestTrue(TEXT("ERA tile consumed"), bERAConsumed);
	}

	// (iii) 80mm glacis at 60 deg (LOS 160mm): residual 340mm.
	{
		const FCEPathElement Path[] = {
			{ LineOfSightThickness(80.0, 60.0), 1.0, false, 0.0 },
		};
		bool bERAConsumed = false;
		const double Residual = SolveCEResidualMM(JetPenMM, Path, 0.0, bERAConsumed);
		TestTrue(TEXT("PG-7VL vs glacis @60 residual ~340mm"), FMath::IsNearlyEqual(Residual, 340.0, 5.0));
	}

	// Standoff degradation kicks in past 400mm and floors at 0.4.
	TestTrue(TEXT("No degradation at contact"), FMath::IsNearlyEqual(CEStandoffFactor(100.0), 1.0, 1e-6));
	TestTrue(TEXT("Degraded at 900mm standoff"), CEStandoffFactor(900.0) < 0.85);
	TestTrue(TEXT("Floor at extreme standoff"), FMath::IsNearlyEqual(CEStandoffFactor(100000.0), 0.4, 1e-6));

	// HESH: scabs monolithic plate under 1.3 calibers, defeated by liners/composite.
	TestTrue(TEXT("HESH scabs 100mm plate with 120mm shell"), HESHProducesScab(100.0, 120.0, 1.0));
	TestFalse(TEXT("HESH defeated by thick plate"), HESHProducesScab(200.0, 120.0, 1.0));
	TestFalse(TEXT("HESH defeated by lined array"), HESHProducesScab(100.0, 120.0, 0.15));

	// Blast: 1 kg TNT ~ 25mm RHA contact defeat.
	TestTrue(TEXT("1kg TNT blast ~25mm"), FMath::IsNearlyEqual(BlastPenetrationMM(1.0), 25.0, 0.01));
	TestTrue(TEXT("8kg TNT blast ~50mm"), FMath::IsNearlyEqual(BlastPenetrationMM(8.0), 50.0, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForgeArmorEstimateMathTest, "Forge.Armor.Math.Estimates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FForgeArmorEstimateMathTest::RunTest(const FString& Parameters)
{
	// Auto-estimate calibration point: .50 AP class projectile lands near 25mm.
	const double FiftyCal = EstimatePenetrationRefMM(EForgePenetratorClass::AP, 0.0459, 12.7, 850.0);
	TestTrue(TEXT(".50 estimate ~25mm"), FiftyCal > 21.0 && FiftyCal < 29.0);

	// Rifle ball stays single-digit.
	const double RifleBall = EstimatePenetrationRefMM(EForgePenetratorClass::FMJ, 0.0095, 7.62, 850.0);
	TestTrue(TEXT("7.62 ball estimate < 10mm"), RifleBall > 2.0 && RifleBall < 10.0);

	// Solid angle utilities behave.
	TestTrue(TEXT("Cone solid angle at 90deg is 2pi"), FMath::IsNearlyEqual(ConeSolidAngleSr(90.0), 2.0 * UE_DOUBLE_PI, 1e-6));
	TestTrue(TEXT("Solid angle shrinks with distance"),
		ApproxSolidAngleSr(FVector(30, 30, 30), 100.0) > ApproxSolidAngleSr(FVector(30, 30, 30), 400.0));
	TestTrue(TEXT("Point blank clamps to hemisphere"),
		FMath::IsNearlyEqual(ApproxSolidAngleSr(FVector(30, 30, 30), 0.5), 2.0 * UE_DOUBLE_PI, 1e-6));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
