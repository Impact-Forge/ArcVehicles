// Copyright Impact-Forge. Pure drone math - no UObjects, unit-testable.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"

/**
 * ForgeDrone
 *
 * The numeric core of the drone systems, kept free of UObjects so it can be exercised by automation
 * tests without spawning a world. The vehicle and component classes are thin wrappers over this.
 *
 * Units: metres, seconds, kilograms, newtons, watts, watt-hours, degrees.
 */
namespace ForgeDrone
{
	// ------------------------------------------------------------------ multirotor mixing

	namespace Multirotor
	{
		/** Placement and spin direction of one motor, relative to the airframe origin. */
		struct FMotorGeometry
		{
			/** Motor position in centimetres, vehicle-local. +X forward, +Y right. */
			FVector RelativeLocationCm = FVector::ZeroVector;

			/** Propeller rotation direction, which sets the sign of its yaw reaction torque. */
			bool bClockwise = false;

			/** Peak thrust this motor can produce, newtons. */
			float MaxThrustN = 8.f;

			/** Motors are shot off, burn out, or fail; a disabled motor contributes nothing. */
			bool bEnabled = true;
		};

		/** Per-axis authority: how much of the throttle range each attitude command may claim. */
		struct FMixAuthority
		{
			float Pitch = 0.35f;
			float Roll = 0.35f;
			float Yaw = 0.25f;
		};

		/**
		 * X-configuration motor mixer.
		 *
		 * Commands are normalised [-1, 1] in aircraft convention: positive pitch raises the nose,
		 * positive roll drops the right side, positive yaw turns the nose right.
		 *
		 * Saturation uses the "airmode" approach: when the mixed set would clip, the whole set is
		 * shifted before clamping, so attitude authority survives at full or zero throttle instead
		 * of the aircraft going uncontrollable exactly when it is worked hardest.
		 *
		 * @param Throttle    Collective demand, [0, 1].
		 * @param OutOutputs  Resized to Motors.Num(); each entry is that motor's demand in [0, 1].
		 */
		FORGEVEHICLESDRONES_API void MixMotors(
			float Throttle,
			float PitchCommand,
			float RollCommand,
			float YawCommand,
			TArrayView<const FMotorGeometry> Motors,
			const FMixAuthority& Authority,
			TArray<float>& OutOutputs);

		/** Throttle fraction that exactly cancels weight, given total available thrust. */
		FORGEVEHICLESDRONES_API float HoverThrottle(float MassKg, float TotalMaxThrustN, float GravityMS2 = 9.81f);

		/** Sum of MaxThrustN across enabled motors. */
		FORGEVEHICLESDRONES_API float TotalAvailableThrustN(TArrayView<const FMotorGeometry> Motors);

		/** Thrust-to-weight ratio at full throttle; below 1 the aircraft cannot hover. */
		FORGEVEHICLESDRONES_API float ThrustToWeightRatio(float MassKg, TArrayView<const FMotorGeometry> Motors, float GravityMS2 = 9.81f);
	}

	// ------------------------------------------------------------------ battery / endurance

	namespace Battery
	{
		/**
		 * Electrical load from the propulsion system.
		 *
		 * Propeller shaft power rises faster than thrust (roughly with thrust^1.5), which is why
		 * hovering is cheap compared to climbing and why aggressive flying drains a pack so fast.
		 *
		 * @param MeanMotorOutput Average of the per-motor demands, [0, 1].
		 */
		FORGEVEHICLESDRONES_API float PropulsionLoadW(float MeanMotorOutput, float MaxPropulsionW);

		/** Charge remaining after drawing LoadW for DeltaSeconds, clamped at empty. */
		FORGEVEHICLESDRONES_API float IntegrateChargeWh(float CurrentWh, float LoadW, float DeltaSeconds);

		/** Flight time at a sustained load, in minutes. Zero load reports zero (not infinity). */
		FORGEVEHICLESDRONES_API float EnduranceMinutes(float CapacityWh, float AverageLoadW);
	}

	// ------------------------------------------------------------------ control link

	namespace Link
	{
		/**
		 * Signal strength from range alone, [0, 1].
		 *
		 * Full quality is held out to FullQualityFraction of the maximum range, then rolls off
		 * smoothly to zero at the limit, rather than degrading from the first metre.
		 */
		FORGEVEHICLESDRONES_API float RangeFactor(float DistanceM, float MaxRangeM, float FullQualityFraction = 0.6f);

		/**
		 * Jamming contribution of a single emitter, [0, 1], where 1 fully denies the link.
		 *
		 * @param DistanceM  Distance from the emitter to the *weaker* end of the link (whichever of
		 *                   drone or operator is closer to it) - jamming attacks the weak end.
		 */
		FORGEVEHICLESDRONES_API float JamContribution(float DistanceM, float RadiusM, float Power, float FalloffExponent = 2.f);

		/** Combined link quality, [0, 1]. */
		FORGEVEHICLESDRONES_API float CombineQuality(float RangeFactor, float LineOfSightFactor, float JamFactor);

		/**
		 * Exponential smoothing used to stop link quality flickering as thin obstructions (a tree,
		 * a passing wall) cross the line of sight.
		 */
		FORGEVEHICLESDRONES_API float SmoothTowards(float Current, float Target, float SmoothingPerSecond, float DeltaSeconds);
	}
}
