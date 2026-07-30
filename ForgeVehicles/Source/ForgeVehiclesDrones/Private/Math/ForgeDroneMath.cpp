// Copyright Impact-Forge. Drone math implementation.

#include "Math/ForgeDroneMath.h"

namespace ForgeDrone::Multirotor
{
	void MixMotors(
		const float Throttle,
		const float PitchCommand,
		const float RollCommand,
		const float YawCommand,
		TArrayView<const FMotorGeometry> Motors,
		const FMixAuthority& Authority,
		TArray<float>& OutOutputs)
	{
		OutOutputs.SetNumZeroed(Motors.Num());
		if (Motors.Num() == 0)
		{
			return;
		}

		const float ClampedThrottle = FMath::Clamp(Throttle, 0.f, 1.f);

		float MinOutput = TNumericLimits<float>::Max();
		float MaxOutput = TNumericLimits<float>::Lowest();
		bool bAnyEnabled = false;

		for (int32 Index = 0; Index < Motors.Num(); ++Index)
		{
			const FMotorGeometry& Motor = Motors[Index];
			if (!Motor.bEnabled)
			{
				OutOutputs[Index] = 0.f;
				continue;
			}

			// Geometry signs: +X is forward, +Y is right. A motor sitting on an axis (plus-config
			// rather than X-config) gets a zero sign there, which correctly excludes it from that
			// axis's authority.
			const float ForwardSign = static_cast<float>(FMath::Sign(Motor.RelativeLocationCm.X));
			const float RightSign = static_cast<float>(FMath::Sign(Motor.RelativeLocationCm.Y));
			// A clockwise propeller pushes the airframe anticlockwise, so increasing it yaws left.
			const float SpinSign = Motor.bClockwise ? -1.f : 1.f;

			// Nose up means rear motors work harder, so front motors take the negative share.
			// Right side down likewise means the right motors ease off.
			const float Mixed = ClampedThrottle
				+ PitchCommand * -ForwardSign * Authority.Pitch
				+ RollCommand * -RightSign * Authority.Roll
				+ YawCommand * SpinSign * Authority.Yaw;

			OutOutputs[Index] = Mixed;
			MinOutput = FMath::Min(MinOutput, Mixed);
			MaxOutput = FMath::Max(MaxOutput, Mixed);
			bAnyEnabled = true;
		}

		if (!bAnyEnabled)
		{
			return;
		}

		// Airmode: shift the entire set back into range before clamping, so the differences between
		// motors (which is what actually produces attitude authority) are preserved.
		const float Overshoot = FMath::Max(0.f, MaxOutput - 1.f);
		const float Undershoot = FMath::Max(0.f, -MinOutput);
		const float Shift = Undershoot - Overshoot;

		for (int32 Index = 0; Index < Motors.Num(); ++Index)
		{
			if (!Motors[Index].bEnabled)
			{
				continue;
			}
			OutOutputs[Index] = FMath::Clamp(OutOutputs[Index] + Shift, 0.f, 1.f);
		}
	}

	float TotalAvailableThrustN(TArrayView<const FMotorGeometry> Motors)
	{
		float Total = 0.f;
		for (const FMotorGeometry& Motor : Motors)
		{
			if (Motor.bEnabled)
			{
				Total += FMath::Max(Motor.MaxThrustN, 0.f);
			}
		}
		return Total;
	}

	float HoverThrottle(const float MassKg, const float TotalMaxThrustN, const float GravityMS2)
	{
		if (TotalMaxThrustN <= UE_SMALL_NUMBER)
		{
			return 1.f;
		}
		return FMath::Clamp((MassKg * GravityMS2) / TotalMaxThrustN, 0.f, 1.f);
	}

	float ThrustToWeightRatio(const float MassKg, TArrayView<const FMotorGeometry> Motors, const float GravityMS2)
	{
		const float Weight = MassKg * GravityMS2;
		if (Weight <= UE_SMALL_NUMBER)
		{
			return TNumericLimits<float>::Max();
		}
		return TotalAvailableThrustN(Motors) / Weight;
	}
}

namespace ForgeDrone::Battery
{
	float PropulsionLoadW(const float MeanMotorOutput, const float MaxPropulsionW)
	{
		const float Output = FMath::Clamp(MeanMotorOutput, 0.f, 1.f);
		return FMath::Max(MaxPropulsionW, 0.f) * FMath::Pow(Output, 1.5f);
	}

	float CruisePropulsionLoadW(const float Throttle, const float MaxPropulsionW)
	{
		const float Demand = FMath::Clamp(Throttle, 0.f, 1.f);
		return FMath::Max(MaxPropulsionW, 0.f) * Demand;
	}

	float IntegrateChargeWh(const float CurrentWh, const float LoadW, const float DeltaSeconds)
	{
		const float DrawnWh = FMath::Max(LoadW, 0.f) * (FMath::Max(DeltaSeconds, 0.f) / 3600.f);
		return FMath::Max(CurrentWh - DrawnWh, 0.f);
	}

	float EnduranceMinutes(const float CapacityWh, const float AverageLoadW)
	{
		if (AverageLoadW <= UE_SMALL_NUMBER || CapacityWh <= 0.f)
		{
			return 0.f;
		}
		return (CapacityWh / AverageLoadW) * 60.f;
	}
}

namespace ForgeDrone::Link
{
	float RangeFactor(const float DistanceM, const float MaxRangeM, const float FullQualityFraction)
	{
		if (MaxRangeM <= UE_SMALL_NUMBER)
		{
			return 0.f;
		}
		if (DistanceM >= MaxRangeM)
		{
			return 0.f;
		}

		const float FullRange = MaxRangeM * FMath::Clamp(FullQualityFraction, 0.f, 0.99f);
		if (DistanceM <= FullRange)
		{
			return 1.f;
		}

		// Smooth roll-off across the outer band rather than a linear cliff.
		const float Alpha = (DistanceM - FullRange) / FMath::Max(MaxRangeM - FullRange, UE_SMALL_NUMBER);
		return FMath::Clamp(1.f - FMath::Square(Alpha), 0.f, 1.f);
	}

	float JamContribution(const float DistanceM, const float RadiusM, const float Power, const float FalloffExponent)
	{
		if (RadiusM <= UE_SMALL_NUMBER || DistanceM >= RadiusM)
		{
			return 0.f;
		}
		const float Normalised = FMath::Clamp(DistanceM / RadiusM, 0.f, 1.f);
		const float Falloff = 1.f - FMath::Pow(Normalised, FMath::Max(FalloffExponent, UE_SMALL_NUMBER));
		return FMath::Clamp(FMath::Max(Power, 0.f) * Falloff, 0.f, 1.f);
	}

	float CombineQuality(const float InRangeFactor, const float LineOfSightFactor, const float JamFactor)
	{
		const float Quality = FMath::Clamp(InRangeFactor, 0.f, 1.f)
			* FMath::Clamp(LineOfSightFactor, 0.f, 1.f)
			* (1.f - FMath::Clamp(JamFactor, 0.f, 1.f));
		return FMath::Clamp(Quality, 0.f, 1.f);
	}

	float SmoothTowards(const float Current, const float Target, const float SmoothingPerSecond, const float DeltaSeconds)
	{
		if (SmoothingPerSecond <= 0.f || DeltaSeconds <= 0.f)
		{
			return Target;
		}
		const float Alpha = FMath::Clamp(SmoothingPerSecond * DeltaSeconds, 0.f, 1.f);
		return FMath::Lerp(Current, Target, Alpha);
	}
}
