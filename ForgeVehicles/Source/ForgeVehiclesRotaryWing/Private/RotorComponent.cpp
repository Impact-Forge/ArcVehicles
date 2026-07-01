//Copyright 2024 H.Kallisto

#include "RotorComponent.h"
#include "DrawDebugHelpers.h"

void URotorComponent::BeginPlay()
{
	Super::BeginPlay();
	DynamicThrust = Thrust;
}

void URotorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void URotorComponent::UpdatePhysics(float deltaTime, UStaticMeshComponent* physicsBody, float takeoffRatio)
{
	if (Implementation == EImplementation::EPhysicsOnly || Implementation == EImplementation::EPhysicsWithAnimation)
	{
		FVector u = this->GetUpVector();
		FVector r = this->GetRightVector();
		FVector f = this->GetForwardVector();

		float direction = bInvertForceDirection ? -1.f : 1.f;

		FVector unit = FVector::ZeroVector;
		switch (RotationAxis)
		{
		case ERotorAxis::X:
			unit = f;
			break;
		case ERotorAxis::Y:
			unit = r;
			break;
		case ERotorAxis::Z:
			unit = u;
			break;
		default:
			break;
		}

		FVector force = FVector::ZeroVector;
		force = unit * 100.f * Thrust * direction;

		/*
		switch (ThrustImplementation)
		{
		case EThrustImplementation::ETI_Constant:
			force = unit * 100.f * Thrust * direction;
			break;
		case EThrustImplementation::ETI_Dynamic:
			force = unit * 100.f * DynamicThrust * direction;
			break;
		}
		*/

		FVector l = GetComponentLocation();
		physicsBody->AddForceAtLocation(force, l, NAME_None);

		if (bShowDebugData)
		{

			DrawDebugLine(GetWorld(), l, l + (force/DebugScale), FColor::Red, false, -1.f, (uint8)0U, LineThickness);
		}
	}
}

void URotorComponent::UpdateAnimation(float currentSpeed, float takeoffRatio)
{
	if (Implementation == EImplementation::EAnimationOnly || Implementation == EImplementation::EPhysicsWithAnimation)
	{
		FVector2D in = FVector2D(0.f, 100.f);
		FVector2D out = FVector2D(MinRotorSpeed, MaxRotorSpeed);

		float direction = bInvertDirection ? -1.f : 1.f;
		float omega = FMath::GetMappedRangeValueClamped(in, out, FMath::Abs(currentSpeed)) * direction;

		FRotator rotation = FRotator::ZeroRotator;

		switch (RotationAxis)
		{
			case ERotorAxis::X:
				rotation = FRotator(0.f, 0.f, omega);
				break;
			case ERotorAxis::Y:
				rotation = FRotator(omega, 0.f, 0.f);
				break;
			case ERotorAxis::Z:
				rotation = FRotator(0.f, omega, 0.f);
				break;
		}

		float scalar = bAffectedByTakeoffAndLanding ? takeoffRatio : 1.f;
		AddLocalRotation(rotation * scalar * RotorSpeedMultiplier);
	}
}