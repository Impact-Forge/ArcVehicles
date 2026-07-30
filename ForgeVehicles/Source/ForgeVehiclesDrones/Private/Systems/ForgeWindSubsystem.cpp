// Copyright Impact-Forge. Wind subsystem implementation.

#include "Systems/ForgeWindSubsystem.h"

#include "Engine/World.h"

UForgeWindSubsystem* UForgeWindSubsystem::Get(const UObject* WorldContextObject)
{
	if (WorldContextObject)
	{
		if (const UWorld* World = WorldContextObject->GetWorld())
		{
			return World->GetSubsystem<UForgeWindSubsystem>();
		}
	}
	return nullptr;
}

void UForgeWindSubsystem::SetWindFromHeading(const float HeadingDegrees, const float SpeedMS)
{
	const float HeadingRad = FMath::DegreesToRadians(HeadingDegrees);
	// Heading is the direction the wind blows towards, measured clockwise from +X (north).
	BaseWindCmS = FVector(FMath::Cos(HeadingRad), FMath::Sin(HeadingRad), 0.f) * SpeedMS * 100.f;
}

void UForgeWindSubsystem::SetGusting(const float NewGustAmplitudeCmS, const float NewGustFrequencyHz)
{
	GustAmplitudeCmS = FMath::Max(NewGustAmplitudeCmS, 0.f);
	GustFrequencyHz = FMath::Max(NewGustFrequencyHz, 0.f);
}

FVector UForgeWindSubsystem::SampleWind(const FVector& WorldPosition) const
{
	if (GustAmplitudeCmS <= 0.f || !GetWorld())
	{
		return BaseWindCmS;
	}

	const float Time = GetWorld()->GetTimeSeconds();
	const float Scale = FMath::Max(TurbulenceScaleCm, 1.f);

	// Layered sines over position and time: cheap, continuous, and identical on every machine
	// without needing a shared random stream.
	const float PhaseX = static_cast<float>(WorldPosition.X) / Scale;
	const float PhaseY = static_cast<float>(WorldPosition.Y) / Scale;
	const float PhaseZ = static_cast<float>(WorldPosition.Z) / Scale;
	const float T = Time * GustFrequencyHz * 2.f * PI;

	const float GustX = FMath::Sin(T + PhaseY * 1.7f) * 0.6f + FMath::Sin(T * 0.37f + PhaseZ) * 0.4f;
	const float GustY = FMath::Cos(T * 0.83f + PhaseX * 1.3f) * 0.6f + FMath::Sin(T * 0.53f + PhaseZ * 0.9f) * 0.4f;
	// Vertical gusting is weaker, which is what real thermals and rotor-wash feel like to a pilot.
	const float GustZ = FMath::Sin(T * 0.61f + PhaseX * 0.8f + PhaseY * 0.6f) * 0.35f;

	return BaseWindCmS + FVector(GustX, GustY, GustZ) * GustAmplitudeCmS;
}

FVector UForgeWindSubsystem::ComputeWindForce(const FVector& WorldPosition, const FVector& VelocityCmS, const float DragCoefficient, const float DragArea, const float AirDensity) const
{
	if (DragArea <= 0.f || DragCoefficient <= 0.f)
	{
		return FVector::ZeroVector;
	}

	// Airspeed is the wind's velocity relative to the body: a drone matching the wind feels nothing,
	// while one holding station against it is pushed continuously.
	const FVector RelativeVelocityMS = (SampleWind(WorldPosition) - VelocityCmS) / 100.f;
	const float Speed = RelativeVelocityMS.Size();
	if (Speed <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	// F = 0.5 * rho * Cd * A * v^2, along the relative flow. Converted to Unreal's cm-based units.
	const float MagnitudeN = 0.5f * AirDensity * DragCoefficient * DragArea * Speed * Speed;
	return RelativeVelocityMS.GetSafeNormal() * MagnitudeN * 100.f;
}
