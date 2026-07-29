// K2 FixedWing Physics

#include "ForgeAeroEngineComponent.h"
#include "DrawDebugHelpers.h"

// Builds a rotation of AngleDeg about the given local axis using Unreal's FRotator conventions
// (Pitch about Y is nose-up, Yaw about Z, Roll about X). Using FRotator avoids the left-handed
// axis-angle sign surprise where FQuat(RightVector, +90) would point a tilted thrust axis downward.
static FRotator K2FW_AxisRotator(EForgeFixedWingAxis Axis, float AngleDeg)
{
	switch (Axis)
	{
	case EForgeFixedWingAxis::X: return FRotator(0.f, 0.f, AngleDeg); // Roll
	case EForgeFixedWingAxis::Y: return FRotator(AngleDeg, 0.f, 0.f); // Pitch
	case EForgeFixedWingAxis::Z: return FRotator(0.f, AngleDeg, 0.f); // Yaw
	default: return FRotator::ZeroRotator;
	}
}

UForgeAeroEngineComponent::UForgeAeroEngineComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // Driven by the owning AForgeFixedWingVehicle.

	// This mesh is visual / a thrust source - thrust is applied to the aircraft body, not via collision.
	// Disabling collision also prevents it welding into the simulated body so it can tilt / spin freely.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}

void UForgeAeroEngineComponent::BeginPlay()
{
	Super::BeginPlay();

	BaseRelativeRotation = GetRelativeRotation().Quaternion();
	CurrentThrottle = 0.f;
	SpinAngle = 0.f;

	// Give jets a slower default spool if the designer left the propeller default in place.
	if (EngineType == EForgeFixedWingEngineType::Jet && FMath::IsNearlyEqual(ThrottleResponse, 4.f))
	{
		ThrottleResponse = 1.f;
	}
}

FVector UForgeAeroEngineComponent::AxisVector(EForgeFixedWingAxis Axis) const
{
	switch (Axis)
	{
	case EForgeFixedWingAxis::X: return GetForwardVector();
	case EForgeFixedWingAxis::Y: return GetRightVector();
	case EForgeFixedWingAxis::Z: return GetUpVector();
	default: return GetForwardVector();
	}
}

void UForgeAeroEngineComponent::UpdateEngine(float DeltaTime, UStaticMeshComponent* Body, float Throttle, float PitchInput, float RollInput, float YawInput, float Airspeed, float TiltAlpha, float DensityRatio)
{
	const bool bHasPhysics = (Mode == EForgeFixedWingEngineMode::PhysicsOnly || Mode == EForgeFixedWingEngineMode::PhysicsWithAnimation);
	const bool bHasAnimation = (Mode == EForgeFixedWingEngineMode::AnimationOnly || Mode == EForgeFixedWingEngineMode::PhysicsWithAnimation);

	// --- Spool the throttle (with differential-thrust mixing) -------------------------------------
	float commanded = Throttle * ThrottleMultiplier + PitchInput * PitchMix + RollInput * RollMix + YawInput * YawMix;
	commanded = FMath::Clamp(commanded, 0.f, 1.f);
	if (!bEngineEnabled)
	{
		commanded = 0.f;
	}
	CurrentThrottle = FMath::FInterpTo(CurrentThrottle, commanded, DeltaTime, ThrottleResponse);

	// --- Apply tilt + thrust-vector + spin to the mesh --------------------------------------------
	// Composed as Base * Tilt * Vector * Spin: the nacelle tilts (VTOL), the nozzle deflects for vectoring,
	// then the prop spins about the resulting shaft. Thrust is later read from the post-rotation vectors.
	if (bCanTilt || bHasAnimation || bThrustVectoring)
	{
		const float tiltAngle = bCanTilt ? (bInvertTilt ? -1.f : 1.f) * TiltAlpha * MaxTiltAngle : 0.f;

		float vectorPitch = 0.f;
		float vectorYaw = 0.f;
		if (bThrustVectoring)
		{
			vectorPitch = FMath::Clamp(PitchInput * PitchVectorMix + RollInput * RollVectorMix, -1.f, 1.f) * MaxVectorAngle;
			vectorYaw = FMath::Clamp(YawInput * YawVectorMix, -1.f, 1.f) * MaxVectorAngle;
		}

		if (bHasAnimation)
		{
			const float spinSpeed = FMath::Lerp(MinSpinSpeed, MaxSpinSpeed, CurrentThrottle) * SpinMultiplier * (bInvertSpin ? -1.f : 1.f);
			SpinAngle = FMath::Fmod(SpinAngle + spinSpeed * DeltaTime, 360.f);
		}

		const FQuat tiltQuat = K2FW_AxisRotator(TiltAxis, tiltAngle).Quaternion();
		const FQuat vectorQuat = bThrustVectoring ? FRotator(vectorPitch, vectorYaw, 0.f).Quaternion() : FQuat::Identity;
		const FQuat spinQuat = bHasAnimation ? K2FW_AxisRotator(SpinAxis, SpinAngle).Quaternion() : FQuat::Identity;

		SetRelativeRotation(BaseRelativeRotation * tiltQuat * vectorQuat * spinQuat);
	}

	// --- Produce thrust ---------------------------------------------------------------------------
	if (!bHasPhysics || Body == nullptr)
	{
		LastThrust = 0.f;
		return;
	}

	float thrustN = MaxThrust * (IdleThrustRatio + (1.f - IdleThrustRatio) * CurrentThrottle);
	if (!bEngineEnabled)
	{
		thrustN = 0.f;
	}

	if (bThrustFalloff)
	{
		const float falloff = FMath::GetMappedRangeValueClamped(FVector2D(0.f, FalloffSpeed), FVector2D(1.f, 1.f - FalloffAmount), FMath::Abs(Airspeed));
		thrustN *= falloff;
	}

	// Air-breathing engines lose thrust with altitude as the air thins.
	thrustN *= FMath::Lerp(1.f, FMath::Clamp(DensityRatio, 0.f, 1.f), ThrustDensityScale);

	LastThrust = thrustN;

	const FVector dir = AxisVector(ThrustAxis) * (bInvertThrust ? -1.f : 1.f);
	const FVector force = dir * thrustN * 100.f; // Newtons -> Unreal force units (kg*cm/s^2).
	const FVector location = GetComponentLocation();
	Body->AddForceAtLocation(force, location);

	if (bShowDebugThrust)
	{
		DrawDebugLine(GetWorld(), location, location + (force / FMath::Max(DebugScale, 1.f)), FColor::Green, false, -1.f, (uint8)0U, DebugLineThickness);
	}
}
