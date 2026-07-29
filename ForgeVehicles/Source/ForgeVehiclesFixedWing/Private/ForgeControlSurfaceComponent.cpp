// K2 FixedWing Physics

#include "ForgeControlSurfaceComponent.h"

// Builds a rotation of AngleDeg about the given local axis using Unreal's FRotator conventions.
static FRotator K2FW_SurfaceRotator(EForgeFixedWingAxis Axis, float AngleDeg)
{
	switch (Axis)
	{
	case EForgeFixedWingAxis::X: return FRotator(0.f, 0.f, AngleDeg); // Roll
	case EForgeFixedWingAxis::Y: return FRotator(AngleDeg, 0.f, 0.f); // Pitch
	case EForgeFixedWingAxis::Z: return FRotator(0.f, AngleDeg, 0.f); // Yaw
	default: return FRotator::ZeroRotator;
	}
}

UForgeControlSurfaceComponent::UForgeControlSurfaceComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // Driven by the owning AForgeFixedWingVehicle.

	// Purely cosmetic - no collision, and never welds into the simulated body.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}

void UForgeControlSurfaceComponent::BeginPlay()
{
	Super::BeginPlay();
	BaseRelativeRotation = GetRelativeRotation().Quaternion();
}

void UForgeControlSurfaceComponent::UpdateSurface(float DeltaTime, float PitchInput, float RollInput, float YawInput, float FlapsInput)
{
	float input = 0.f;
	switch (SurfaceType)
	{
	case EForgeFixedWingControlSurface::Elevator:     input = PitchInput; break;
	case EForgeFixedWingControlSurface::AileronLeft:  input = RollInput; break;
	case EForgeFixedWingControlSurface::AileronRight: input = -RollInput; break;
	case EForgeFixedWingControlSurface::Rudder:       input = YawInput; break;
	case EForgeFixedWingControlSurface::Flap:         input = FlapsInput; break;
	default: break;
	}

	const float target = FMath::Clamp(input, -1.f, 1.f) * MaxDeflection * (bInvert ? -1.f : 1.f);
	CurrentDeflection = FMath::FInterpTo(CurrentDeflection, target, DeltaTime, DeflectionSpeed);

	const FQuat deflectQuat = K2FW_SurfaceRotator(DeflectionAxis, CurrentDeflection).Quaternion();
	SetRelativeRotation(BaseRelativeRotation * deflectQuat);
}
