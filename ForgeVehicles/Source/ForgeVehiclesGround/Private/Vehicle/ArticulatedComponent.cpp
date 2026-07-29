////Copyright 2025 P.Kallisto 


#include "Vehicle/ArticulatedComponent.h"


UArticulatedComponent::UArticulatedComponent()
{

}


void UArticulatedComponent::BeginPlay()
{
	BaseLocation = GetRelativeLocation();
	BaseRotation = GetRelativeRotation();
}


void UArticulatedComponent::Update(float deltaTime)
{
	FRotator modifiedRotator = BaseRotation;
	FVector modifiedLocation = BaseLocation;

	float t = 60.f / FMath::Pow(deltaTime, -1.f);
	Movement = bClamped ? FMath::Clamp(Movement + (InputValue * MovementSpeed * t), Min, Max) : (Movement + (InputValue * MovementSpeed * t));

	if (bUseSmoothing)
	{
		SmoothMovement = FMath::FInterpTo(SmoothMovement, Movement, deltaTime, SmoothingSpeed);
	}
	else
	{
		SmoothMovement = Movement;
	}


	switch (Axis)
	{
	case EArticulationAxis::X:
		modifiedLocation.X = BaseLocation.X + SmoothMovement;
		break;
	case EArticulationAxis::Y:
		modifiedLocation.Y = BaseLocation.Y + SmoothMovement;
		break;
	case EArticulationAxis::Z:
		modifiedLocation.Z = BaseLocation.Z + SmoothMovement;
		break;
	case EArticulationAxis::Roll:
		modifiedRotator.Roll = BaseRotation.Roll + SmoothMovement;
		break;
	case EArticulationAxis::Pitch:
		modifiedRotator.Pitch = BaseRotation.Pitch + SmoothMovement;
		break;
	case EArticulationAxis::Yaw:
		modifiedRotator.Yaw = BaseRotation.Yaw + SmoothMovement;
		break;
	}

	SetRelativeLocationAndRotation(modifiedLocation, modifiedRotator, true, nullptr, ETeleportType::ResetPhysics);
}


void UArticulatedComponent::SetInput(float value)
{
	InputValue = value;
}


float UArticulatedComponent::GetMovement()
{
	return Movement;
}
