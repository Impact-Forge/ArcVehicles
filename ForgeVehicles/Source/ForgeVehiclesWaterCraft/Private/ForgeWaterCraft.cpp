// Copyright Impact-Forge. Water-craft physics authored for Forge Vehicles.

#include "ForgeWaterCraft.h"

#include "ForgeBuoyancyComponent.h"
#include "Components/StaticMeshComponent.h"

AForgeWaterCraft::AForgeWaterCraft(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	Hull->SetCollisionProfileName(TEXT("Vehicle"));
	Hull->SetSimulatePhysics(true);
	Hull->BodyInstance.bUseCCD = true;
	SetRootComponent(Hull);

	Buoyancy = CreateDefaultSubobject<UForgeBuoyancyComponent>(TEXT("Buoyancy"));

	// Boats hold their throttle rather than springing back to zero on release.
	bIsThrottleCollective = true;
}

void AForgeWaterCraft::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!Hull || !Hull->IsSimulatingPhysics())
	{
		return;
	}

	// Only the authority (or an autonomous local driver) applies propulsion forces.
	if (GetLocalRole() < ROLE_AutonomousProxy)
	{
		return;
	}

	if (!bEngineRunning)
	{
		return;
	}

	const FVector Forward = GetActorForwardVector();
	const FVector Velocity = Hull->GetPhysicsLinearVelocity();
	const float ForwardSpeed = FVector::DotProduct(Velocity, Forward);

	// Forward / reverse thrust, capped at MaxSpeed.
	if (!FMath::IsNearlyZero(ThrottleInput) && Velocity.Size() < MaxSpeed)
	{
		Hull->AddForce(Forward * ThrustForce * ThrottleInput);
	}

	// Yaw steering, more effective the faster the craft is moving through the water.
	if (!FMath::IsNearlyZero(SteeringInput))
	{
		const float SpeedFactor = FMath::Clamp(FMath::Abs(ForwardSpeed) / FullTurnAuthoritySpeed, MinTurnAuthority, 1.f);
		const float DirectionSign = ForwardSpeed >= 0.f ? 1.f : -1.f;
		Hull->AddTorqueInRadians(FVector(0.f, 0.f, TurnTorque * SteeringInput * SpeedFactor * DirectionSign));
	}
}
