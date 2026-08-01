// Copyright Impact-Forge. Reconnaissance quadcopter implementation.

#include "Archetypes/ForgeReconQuad.h"

#include "Components/StaticMeshComponent.h"
#include "Payloads/ForgeDroneDropReleaseComponent.h"
#include "Payloads/ForgeDroneGimbalComponent.h"
#include "Systems/ForgeDroneAutopilotComponent.h"
#include "Systems/ForgeDroneBatteryComponent.h"
#include "Systems/ForgeDroneLinkComponent.h"

AForgeReconQuad::AForgeReconQuad(const FObjectInitializer& ObjectInitializer)
	// A sub-kilogram airframe has no cargo hold and nobody boards it. It also should not injure
	// anything by touching it, unlike the strike quad.
	// This belongs in the initialization list, not the body: AForgeVehicle creates these subobjects
	// in its own constructor, which has already finished by the time the body runs. The engine
	// asserts on any subobject setup made after that point.
	: Super(ObjectInitializer
		.DoNotCreateDefaultSubobject(TEXT("VehicleInventory"))
		.DoNotCreateDefaultSubobject(TEXT("RunOverComponent"))
		.DoNotCreateDefaultSubobject(TEXT("OccupantExitPoint")))
{
	// ---- Airframe: ~350 mm diagonal, four modest motors. Thrust-to-weight around 2.4, which is
	// plenty for a camera platform and keeps it from being twitchy.
	ArmLengthCm = 16.f;
	DefaultMotorThrustN = 5.5f;

	// ---- Flight: everything here is for a steady picture rather than agility. Angle mode with a
	// modest tilt limit means releasing the sticks parks it in a hover.
	FlightMode = EForgeMultirotorFlightMode::Angle;
	MaxTiltAngleDeg = 25.f;
	ClimbRateMS = 4.f;
	AngleModeLevelGain = 5.f;
	AcroMaxRatesDps = FVector(220.f, 220.f, 150.f);

	BodyDragCoefficient = 1.1f;
	FrontalAreaM2 = 0.02f;

	// ---- Endurance: ~28 minutes, which is what makes this drone useful for watching rather than
	// dashing. Hover draw lands near 128 W against a 60 Wh pack.
	Battery = CreateDefaultSubobject<UForgeDroneBatteryComponent>(TEXT("Battery"));
	Battery->CapacityWh = 60.f;
	Battery->AvionicsLoadW = 10.f;         // flight controller, radio and camera
	Battery->MaxPropulsionLoadW = 450.f;
	Battery->LowChargeThreshold = 0.25f;   // warn early: it has to fly home from a long way out

	// ---- Link: long range, and it comes home on its own if the signal goes. A camera drone is worth
	// recovering; there is no reason to drop it.
	Link = CreateDefaultSubobject<UForgeDroneLinkComponent>(TEXT("ControlLink"));
	Link->MaxRangeM = 9000.f;
	Link->LinkLossBehavior = EForgeDroneLinkLossBehavior::ReturnToHome;

	Autopilot = CreateDefaultSubobject<UForgeDroneAutopilotComponent>(TEXT("Autopilot"));

	// ---- Payload: a stabilised gimbal, because a fixed camera on a manoeuvring airframe is useless
	// for observation. Full downward travel for looking straight at what is below.
	Gimbal = CreateDefaultSubobject<UForgeDroneGimbalComponent>(TEXT("Gimbal"));
	Gimbal->MinPitchDeg = -90.f;
	Gimbal->MaxPitchDeg = 30.f;
	Gimbal->SlewRateDegPerSecond = 90.f;

	// Two small stores: the field-improvised role these airframes are actually put to. The store class
	// is left for the project to assign.
	Stores = CreateDefaultSubobject<UForgeDroneDropReleaseComponent>(TEXT("Stores"));
	Stores->StoreCapacity = 2;
	Stores->ReleaseIntervalSeconds = 0.75f;
}

void AForgeReconQuad::BeginPlay()
{
	// Mass must be set before the flight model samples it at BeginPlay.
	if (Core)
	{
		Core->SetMassOverrideInKg(NAME_None, AirframeMassKg, true);
	}

	Super::BeginPlay();

	// Where it returns to when the link drops.
	if (Link)
	{
		Link->SetHomeLocation(GetActorLocation());
	}
	if (Autopilot)
	{
		Autopilot->SetHomeLocation(GetActorLocation());
	}
}
