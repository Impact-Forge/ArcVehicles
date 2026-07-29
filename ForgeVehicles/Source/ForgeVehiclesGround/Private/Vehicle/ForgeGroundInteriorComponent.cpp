//Copyright 2025 P.Kallisto 


#include "Vehicle/ForgeGroundInteriorComponent.h"

UForgeGroundInteriorComponent::UForgeGroundInteriorComponent()
{
}

void UForgeGroundInteriorComponent::BeginPlay()
{
	Super::BeginPlay();
	BaseRotation = GetRelativeRotation();

}

void UForgeGroundInteriorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UForgeGroundInteriorComponent::Update(float deltaTime, float inRPM, float inSpeed, float inSteering)
{
	float value = 0.f;
	switch (AnimationType)
	{
	case EForgeGroundAnimationType::Steering:
		value = inSteering;
		break;
	case EForgeGroundAnimationType::RPM:
		value = inRPM;
		break;
	case EForgeGroundAnimationType::Speed:
		value = inSpeed;
		break;
	}

	FRotator newRotator = FRotator::ZeroRotator;
	float d = bInvert ? -1.f : 1.f;

	float remap = FMath::GetMappedRangeValueClamped(InputRange, OutputRange, d*value);


	if (bUseSmoothing)
	{
		Rotation =FMath::FInterpTo(Rotation, remap, deltaTime, SmoothingSpeed);
	}
	else
	{
		Rotation = remap;
	}

	switch (RotationAxis)
	{
	case ERotatorAxis::Yaw:
		newRotator.Yaw = Rotation;
		newRotator.Pitch = BaseRotation.Pitch;
		newRotator.Roll = BaseRotation.Roll;
		break;
	case ERotatorAxis::Roll:
		newRotator.Yaw = BaseRotation.Yaw;
		newRotator.Pitch = BaseRotation.Pitch;
		newRotator.Roll = Rotation;
		break;
	case ERotatorAxis::Pitch:
		newRotator.Yaw = BaseRotation.Yaw;
		newRotator.Pitch = Rotation;
		newRotator.Roll = BaseRotation.Roll;
		break;
	}



	SetRelativeRotation(newRotator);
}
