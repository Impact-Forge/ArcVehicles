// Copyright Impact-Forge. Numeric self-tests for the drone math.

#include "Misc/AutomationTest.h"

#include "Math/ForgeDroneMath.h"
#include "Operator/ForgeDroneInputFrame.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace ForgeDrone;

namespace
{
	/** Standard 250mm-class X quad: front-right CCW, front-left CW, rear-left CCW, rear-right CW. */
	TArray<Multirotor::FMotorGeometry> MakeXQuad(const float MaxThrustN = 8.f)
	{
		TArray<Multirotor::FMotorGeometry> Motors;
		Motors.SetNum(4);

		// 0: front-right, 1: front-left, 2: rear-left, 3: rear-right
		Motors[0].RelativeLocationCm = FVector(12.f, 12.f, 0.f);
		Motors[0].bClockwise = false;
		Motors[1].RelativeLocationCm = FVector(12.f, -12.f, 0.f);
		Motors[1].bClockwise = true;
		Motors[2].RelativeLocationCm = FVector(-12.f, -12.f, 0.f);
		Motors[2].bClockwise = false;
		Motors[3].RelativeLocationCm = FVector(-12.f, 12.f, 0.f);
		Motors[3].bClockwise = true;

		for (Multirotor::FMotorGeometry& Motor : Motors)
		{
			Motor.MaxThrustN = MaxThrustN;
		}
		return Motors;
	}

	bool AllWithinUnitRange(const TArray<float>& Outputs)
	{
		for (const float Output : Outputs)
		{
			if (Output < 0.f || Output > 1.f)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForgeDroneMixerTest, "Forge.Drones.Math.Mixer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FForgeDroneMixerTest::RunTest(const FString& Parameters)
{
	const TArray<Multirotor::FMotorGeometry> Quad = MakeXQuad();
	const Multirotor::FMixAuthority Authority;
	TArray<float> Outputs;

	// ---- Level hover: no attitude command means every motor carries an equal share.
	Multirotor::MixMotors(0.5f, 0.f, 0.f, 0.f, Quad, Authority, Outputs);
	TestEqual(TEXT("Mixer produces one output per motor"), Outputs.Num(), 4);
	for (const float Output : Outputs)
	{
		TestTrue(TEXT("Hover is symmetric across all four motors"), FMath::IsNearlyEqual(Output, 0.5f, 1e-4f));
	}

	// ---- Pitch up: the rear pair must out-thrust the front pair.
	Multirotor::MixMotors(0.5f, 1.f, 0.f, 0.f, Quad, Authority, Outputs);
	const float FrontPitch = (Outputs[0] + Outputs[1]) * 0.5f;
	const float RearPitch = (Outputs[2] + Outputs[3]) * 0.5f;
	TestTrue(TEXT("Nose-up loads the rear motors"), RearPitch > FrontPitch);
	TestTrue(TEXT("Pitch keeps left/right balance"), FMath::IsNearlyEqual(Outputs[0], Outputs[1], 1e-4f));
	TestTrue(TEXT("Pitch outputs stay in range"), AllWithinUnitRange(Outputs));

	// ---- Roll right: the right pair eases off so that side drops.
	Multirotor::MixMotors(0.5f, 0.f, 1.f, 0.f, Quad, Authority, Outputs);
	const float RightRoll = (Outputs[0] + Outputs[3]) * 0.5f;
	const float LeftRoll = (Outputs[1] + Outputs[2]) * 0.5f;
	TestTrue(TEXT("Roll right unloads the right motors"), RightRoll < LeftRoll);
	TestTrue(TEXT("Roll keeps front/rear balance"), FMath::IsNearlyEqual(Outputs[0], Outputs[3], 1e-4f));

	// ---- Yaw right: driven by reaction torque, so the anticlockwise pair spins up.
	Multirotor::MixMotors(0.5f, 0.f, 0.f, 1.f, Quad, Authority, Outputs);
	const float CounterClockwisePair = (Outputs[0] + Outputs[2]) * 0.5f; // motors 0 and 2 are CCW
	const float ClockwisePair = (Outputs[1] + Outputs[3]) * 0.5f;
	TestTrue(TEXT("Yaw right spins up the anticlockwise motors"), CounterClockwisePair > ClockwisePair);
	TestTrue(TEXT("Yaw is diagonally symmetric"), FMath::IsNearlyEqual(Outputs[0], Outputs[2], 1e-4f));

	// ---- Airmode: full throttle plus a pitch command must retain attitude authority rather than
	// flattening to four saturated motors.
	Multirotor::MixMotors(1.f, 1.f, 0.f, 0.f, Quad, Authority, Outputs);
	TestTrue(TEXT("Saturated throttle stays in range"), AllWithinUnitRange(Outputs));
	const float SaturatedFront = (Outputs[0] + Outputs[1]) * 0.5f;
	const float SaturatedRear = (Outputs[2] + Outputs[3]) * 0.5f;
	TestTrue(TEXT("Attitude authority survives full throttle"), SaturatedRear > SaturatedFront);
	TestTrue(TEXT("Airmode preserves the commanded differential"),
		FMath::IsNearlyEqual(SaturatedRear - SaturatedFront, 2.f * Authority.Pitch, 1e-3f));

	// ---- Same at zero throttle: a descending quad must still be steerable.
	Multirotor::MixMotors(0.f, 1.f, 0.f, 0.f, Quad, Authority, Outputs);
	TestTrue(TEXT("Zero throttle stays in range"), AllWithinUnitRange(Outputs));
	TestTrue(TEXT("Attitude authority survives zero throttle"),
		(Outputs[2] + Outputs[3]) > (Outputs[0] + Outputs[1]));

	// ---- Motor-out: a dead motor contributes nothing and the rest stay valid.
	TArray<Multirotor::FMotorGeometry> Damaged = MakeXQuad();
	Damaged[0].bEnabled = false;
	Multirotor::MixMotors(0.5f, 0.f, 0.f, 0.f, Damaged, Authority, Outputs);
	TestEqual(TEXT("Dead motor produces no thrust"), Outputs[0], 0.f);
	TestTrue(TEXT("Surviving motors keep running"), Outputs[1] > 0.f && Outputs[2] > 0.f && Outputs[3] > 0.f);
	TestTrue(TEXT("Motor-out outputs stay in range"), AllWithinUnitRange(Outputs));

	// ---- Empty motor set must not crash or produce output.
	TArray<Multirotor::FMotorGeometry> NoMotors;
	Multirotor::MixMotors(0.5f, 0.f, 0.f, 0.f, NoMotors, Authority, Outputs);
	TestEqual(TEXT("No motors produces no outputs"), Outputs.Num(), 0);

	// ---- Hover throttle and thrust-to-weight.
	// 1.2 kg on 4x8 N: weight 11.77 N against 32 N available.
	TestTrue(TEXT("Hover throttle for a 1.2kg quad on 32N is ~0.368"),
		FMath::IsNearlyEqual(Multirotor::HoverThrottle(1.2f, 32.f), 0.368f, 0.005f));
	TestTrue(TEXT("Thrust-to-weight for the FPV quad is ~2.7"),
		FMath::IsNearlyEqual(Multirotor::ThrustToWeightRatio(1.2f, Quad), 2.719f, 0.01f));
	TestTrue(TEXT("Losing a motor drops thrust-to-weight below 2.1"),
		Multirotor::ThrustToWeightRatio(1.2f, Damaged) < 2.1f);
	TestEqual(TEXT("Zero available thrust demands full throttle"), Multirotor::HoverThrottle(1.2f, 0.f), 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForgeDroneBatteryTest, "Forge.Drones.Math.Battery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FForgeDroneBatteryTest::RunTest(const FString& Parameters)
{
	// ---- Endurance of the four shipped archetypes, derived from their authored constants rather than
	// from pre-computed loads, so that retuning an archetype and forgetting its README figure fails
	// here. Each pack figure is the value set in the matching constructor.
	{
		// Rotorcraft: demand at hover is weight / total available thrust, then the rotor power curve.
		const auto QuadEnduranceMinutes = [](const float MassKg, const float MotorThrustN, const float CapacityWh,
			const float AvionicsW, const float MaxPropulsionW)
		{
			const float HoverDemand = Multirotor::HoverThrottle(MassKg, MotorThrustN * 4.f);
			const float LoadW = AvionicsW + Battery::PropulsionLoadW(HoverDemand, MaxPropulsionW);
			return Battery::EnduranceMinutes(CapacityWh, LoadW);
		};

		// Wings: the propeller only fights drag, so a settled cruise throttle maps onto the cruise curve.
		const auto WingEnduranceMinutes = [](const float CruiseThrottle, const float CapacityWh,
			const float AvionicsW, const float MaxPropulsionW)
		{
			const float LoadW = AvionicsW + Battery::CruisePropulsionLoadW(CruiseThrottle, MaxPropulsionW);
			return Battery::EnduranceMinutes(CapacityWh, LoadW);
		};

		const float FPVMinutes = QuadEnduranceMinutes(1.2f, 8.f, 28.f, 8.f, 1200.f);
		const float ReconMinutes = QuadEnduranceMinutes(0.92f, 5.5f, 60.f, 10.f, 450.f);
		const float UAVMinutes = WingEnduranceMinutes(0.35f, 90.f, 12.f, 200.f);
		const float MunitionMinutes = WingEnduranceMinutes(0.5f, 50.f, 14.f, 240.f);

		TestTrue(TEXT("FPV kamikaze quad hovers for ~6 min"), FMath::IsNearlyEqual(FPVMinutes, 6.f, 0.3f));
		TestTrue(TEXT("Recon quad hovers for ~28 min"), FMath::IsNearlyEqual(ReconMinutes, 28.f, 0.5f));
		TestTrue(TEXT("Fixed-wing UAV cruises for ~66 min"), FMath::IsNearlyEqual(UAVMinutes, 66.f, 0.5f));
		TestTrue(TEXT("Loitering munition cruises for ~22 min"), FMath::IsNearlyEqual(MunitionMinutes, 22.4f, 0.5f));

		// The ordering is the point of the whole set: a wing outlasts a rotor, a scout outlasts a weapon.
		TestTrue(TEXT("The wing outlasts every rotorcraft"), UAVMinutes > ReconMinutes);
		TestTrue(TEXT("Each scout outlasts its armed counterpart"),
			ReconMinutes > FPVMinutes && UAVMinutes > MunitionMinutes);
	}

	// ---- Integration agrees with the closed-form endurance: draining at a fixed load for the
	// predicted flight time must land exactly on empty.
	{
		constexpr float CapacityWh = 28.f;
		constexpr float LoadW = 280.f;
		const float PredictedSeconds = Battery::EnduranceMinutes(CapacityWh, LoadW) * 60.f;

		float Charge = CapacityWh;
		constexpr float Step = 0.1f;
		for (float Elapsed = 0.f; Elapsed < PredictedSeconds - Step; Elapsed += Step)
		{
			Charge = Battery::IntegrateChargeWh(Charge, LoadW, Step);
		}
		TestTrue(TEXT("Integrated drain nearly empties the pack at predicted endurance"), Charge < CapacityWh * 0.01f);
		TestTrue(TEXT("Integrated drain has not gone negative"), Charge >= 0.f);
	}

	// ---- Charge never goes below empty however hard it is driven.
	TestEqual(TEXT("Charge clamps at empty"), Battery::IntegrateChargeWh(0.5f, 10000.f, 10.f), 0.f);
	TestEqual(TEXT("Zero load draws nothing"), Battery::IntegrateChargeWh(10.f, 0.f, 60.f), 10.f);

	// ---- Propulsion load follows a thrust^1.5 curve: hovering is far cheaper than full throttle.
	const float HoverLoad = Battery::PropulsionLoadW(0.37f, 1400.f);
	const float FullLoad = Battery::PropulsionLoadW(1.f, 1400.f);
	TestTrue(TEXT("Full throttle draws the rated propulsion power"), FMath::IsNearlyEqual(FullLoad, 1400.f, 0.01f));
	TestTrue(TEXT("Hover costs well under a third of full throttle"), HoverLoad < FullLoad * 0.3f);
	TestTrue(TEXT("Idle draws no propulsion power"), FMath::IsNearlyEqual(Battery::PropulsionLoadW(0.f, 1400.f), 0.f, 1e-4f));
	TestTrue(TEXT("Load curve is monotonic"),
		Battery::PropulsionLoadW(0.8f, 1400.f) > Battery::PropulsionLoadW(0.5f, 1400.f));

	// ---- The wing's cruise curve is linear in throttle, not the rotor's thrust^1.5. Both agree at the
	// ends; between them the rotor curve is the cheaper of the two, which is exactly why a wing must not
	// borrow it - doing so would flatter its endurance at every part-throttle setting it ever cruises at.
	TestTrue(TEXT("Cruise curve draws the rated power at full throttle"),
		FMath::IsNearlyEqual(Battery::CruisePropulsionLoadW(1.f, 200.f), 200.f, 0.01f));
	TestTrue(TEXT("Cruise curve draws nothing at idle"),
		FMath::IsNearlyEqual(Battery::CruisePropulsionLoadW(0.f, 200.f), 0.f, 1e-4f));
	TestTrue(TEXT("Cruise curve is linear in throttle"),
		FMath::IsNearlyEqual(Battery::CruisePropulsionLoadW(0.35f, 200.f), 70.f, 0.01f));
	TestTrue(TEXT("Cruise curve costs more than the rotor curve at part throttle"),
		Battery::CruisePropulsionLoadW(0.35f, 200.f) > Battery::PropulsionLoadW(0.35f, 200.f));
	TestTrue(TEXT("Cruise throttle clamps rather than over-drawing"),
		FMath::IsNearlyEqual(Battery::CruisePropulsionLoadW(3.f, 200.f), 200.f, 0.01f));
	TestTrue(TEXT("Negative throttle draws nothing"),
		FMath::IsNearlyEqual(Battery::CruisePropulsionLoadW(-1.f, 200.f), 0.f, 1e-4f));

	// ---- Degenerate input reports no endurance rather than infinity.
	TestEqual(TEXT("Zero load reports zero endurance, not infinite"), Battery::EnduranceMinutes(50.f, 0.f), 0.f);
	TestEqual(TEXT("Empty pack reports zero endurance"), Battery::EnduranceMinutes(0.f, 100.f), 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForgeDroneLinkTest, "Forge.Drones.Math.Link",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FForgeDroneLinkTest::RunTest(const FString& Parameters)
{
	constexpr float MaxRange = 5000.f;

	// ---- Range: solid inside the inner band, gone at the limit, monotonic between.
	TestEqual(TEXT("Link is perfect at the operator"), Link::RangeFactor(0.f, MaxRange), 1.f);
	TestEqual(TEXT("Link is perfect within the full-quality band"), Link::RangeFactor(2500.f, MaxRange), 1.f);
	TestEqual(TEXT("Link is dead at maximum range"), Link::RangeFactor(MaxRange, MaxRange), 0.f);
	TestEqual(TEXT("Link is dead beyond maximum range"), Link::RangeFactor(9000.f, MaxRange), 0.f);
	TestTrue(TEXT("Link degrades monotonically through the outer band"),
		Link::RangeFactor(3200.f, MaxRange) > Link::RangeFactor(4200.f, MaxRange));
	TestTrue(TEXT("Outer band is partially degraded"),
		Link::RangeFactor(4000.f, MaxRange) > 0.f && Link::RangeFactor(4000.f, MaxRange) < 1.f);
	TestEqual(TEXT("A zero-range radio has no link"), Link::RangeFactor(10.f, 0.f), 0.f);

	// ---- Jamming: strongest at the emitter, nothing outside its radius, clamped to [0,1].
	TestTrue(TEXT("Jamming is total at the emitter"), FMath::IsNearlyEqual(Link::JamContribution(0.f, 500.f, 1.f), 1.f, 1e-4f));
	TestEqual(TEXT("Jamming stops at the radius"), Link::JamContribution(500.f, 500.f, 1.f), 0.f);
	TestEqual(TEXT("Jamming does not reach beyond the radius"), Link::JamContribution(900.f, 500.f, 1.f), 0.f);
	TestTrue(TEXT("Jamming falls off with distance"),
		Link::JamContribution(100.f, 500.f, 1.f) > Link::JamContribution(400.f, 500.f, 1.f));
	TestTrue(TEXT("A weak emitter cannot fully deny the link"), Link::JamContribution(0.f, 500.f, 0.4f) <= 0.4f + 1e-4f);
	TestTrue(TEXT("Over-powered emitters clamp at total denial"), Link::JamContribution(0.f, 500.f, 5.f) <= 1.f);

	// ---- Combination.
	TestTrue(TEXT("Clear, close, unjammed is a perfect link"),
		FMath::IsNearlyEqual(Link::CombineQuality(1.f, 1.f, 0.f), 1.f, 1e-4f));
	TestEqual(TEXT("Full jamming kills the link outright"), Link::CombineQuality(1.f, 1.f, 1.f), 0.f);
	TestEqual(TEXT("Total occlusion kills the link outright"), Link::CombineQuality(1.f, 0.f, 0.f), 0.f);
	TestTrue(TEXT("Degradations compound"),
		FMath::IsNearlyEqual(Link::CombineQuality(0.5f, 0.5f, 0.5f), 0.125f, 1e-4f));

	// ---- Smoothing approaches the target without overshooting.
	const float Smoothed = Link::SmoothTowards(0.f, 1.f, 4.f, 0.1f);
	TestTrue(TEXT("Smoothing moves toward the target"), Smoothed > 0.f && Smoothed < 1.f);
	TestEqual(TEXT("A large step snaps to the target"), Link::SmoothTowards(0.f, 1.f, 100.f, 1.f), 1.f);
	TestEqual(TEXT("Zero smoothing is instant"), Link::SmoothTowards(0.f, 1.f, 0.f, 0.1f), 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForgeDroneInputFrameTest, "Forge.Drones.Relay.Frame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FForgeDroneInputFrameTest::RunTest(const FString& Parameters)
{
	using FFrame = FForgeDroneInputFrame;

	// ---- Quantisation: the extremes and the centre must survive exactly, since those are the positions
	// a pilot actually holds - full deflection and hands off.
	TestEqual(TEXT("Centre survives the round trip exactly"), FFrame::Dequantise(FFrame::Quantise(0.f)), 0.f);
	TestEqual(TEXT("Full deflection survives exactly"), FFrame::Dequantise(FFrame::Quantise(1.f)), 1.f);
	TestEqual(TEXT("Full reverse deflection survives exactly"), FFrame::Dequantise(FFrame::Quantise(-1.f)), -1.f);

	// A byte per axis is worth about 0.8% of full travel, which is finer than any stick reports.
	for (float Value = -1.f; Value <= 1.f; Value += 0.05f)
	{
		const float RoundTripped = FFrame::Dequantise(FFrame::Quantise(Value));
		TestTrue(TEXT("Quantisation error stays under one part in a hundred"),
			FMath::Abs(RoundTripped - Value) < 0.01f);
	}

	// ---- Out-of-range input is clamped rather than wrapping into the opposite deflection, which is the
	// failure that would turn a hard right into a hard left.
	TestEqual(TEXT("Over-range input clamps to full"), FFrame::Dequantise(FFrame::Quantise(5.f)), 1.f);
	TestEqual(TEXT("Under-range input clamps to full reverse"), FFrame::Dequantise(FFrame::Quantise(-5.f)), -1.f);

	// ---- Sequence ordering, including the wrap at 256. This is the whole reason the counter exists:
	// unreliable frames arrive out of order, and getting the comparison wrong drags the aircraft back
	// through inputs the pilot has already left behind.
	{
		FFrame Frame;

		Frame.Sequence = 5;
		TestTrue(TEXT("A later frame is newer"), Frame.IsNewerThan(4));
		TestFalse(TEXT("An earlier frame is not newer"), Frame.IsNewerThan(6));
		TestFalse(TEXT("A repeated frame is not newer"), Frame.IsNewerThan(5));

		// Across the wrap: 2 arrives after 250 and must count as newer.
		Frame.Sequence = 2;
		TestTrue(TEXT("A frame just past the wrap is newer than one just before it"), Frame.IsNewerThan(250));

		// And the reverse must not be true, or a wrap would let a stale frame through.
		Frame.Sequence = 250;
		TestFalse(TEXT("A frame just before the wrap is not newer than one just past it"), Frame.IsNewerThan(2));

		// Half the range apart is the ambiguous case; anything beyond it is treated as older, which is
		// correct because a gap that large means far more loss than a control stream can survive anyway.
		Frame.Sequence = 100;
		TestTrue(TEXT("A frame 60 ahead is newer"), Frame.IsNewerThan(40));
		TestFalse(TEXT("A frame 60 behind is not newer"), Frame.IsNewerThan(160));
	}

	// ---- A default frame is centred sticks, which is what a released or timed-out drone gets handed.
	{
		const FFrame Default;
		TestEqual(TEXT("A default frame has no longitudinal input"), FFrame::Dequantise(Default.Longitudinal), 0.f);
		TestEqual(TEXT("A default frame has no lateral input"), FFrame::Dequantise(Default.Lateral), 0.f);
		TestEqual(TEXT("A default frame has no yaw input"), FFrame::Dequantise(Default.Yaw), 0.f);
		TestEqual(TEXT("A default frame has no vertical input"), FFrame::Dequantise(Default.Vertical), 0.f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
