// Copyright Impact-Forge. FPV strike quadcopter implementation.

#include "Archetypes/ForgeFPVKamikazeQuad.h"

#include "Components/StaticMeshComponent.h"
#include "Payloads/ForgeDroneWarheadComponent.h"
#include "Systems/ForgeDroneBatteryComponent.h"
#include "Systems/ForgeDroneLinkComponent.h"

AForgeFPVKamikazeQuad::AForgeFPVKamikazeQuad(const FObjectInitializer& ObjectInitializer)
	// No cargo, no exit point - but the run-over component is deliberately kept. A five-inch quad
	// arriving at forty metres a second is a physical impact whether or not the warhead functions.
	// This belongs in the initialization list, not the body: AForgeVehicle creates these subobjects
	// in its own constructor, which has already finished by the time the body runs. The engine
	// asserts on any subobject setup made after that point.
	: Super(ObjectInitializer
		.DoNotCreateDefaultSubobject(TEXT("VehicleInventory"))
		.DoNotCreateDefaultSubobject(TEXT("OccupantExitPoint")))
{
	// ---- Airframe: ~250 mm diagonal five-inch class. Thrust-to-weight near 2.7 including the
	// warhead, which is what gives it the acceleration to run in.
	ArmLengthCm = 12.f;
	DefaultMotorThrustN = 8.f;

	// ---- Flight: Acro by default. The sticks command rotation rates with no self-levelling, which is
	// what allows continuous rolls and dives, and is how these are actually flown.
	FlightMode = EForgeMultirotorFlightMode::Acro;
	AcroMaxRatesDps = FVector(720.f, 720.f, 400.f);
	MaxTiltAngleDeg = 55.f;   // used only if a pilot switches to Angle mode
	ClimbRateMS = 8.f;

	// Higher control authority than the camera drone: it has to change direction hard.
	PitchAuthority = 0.4f;
	RollAuthority = 0.4f;
	YawAuthority = 0.3f;

	BodyDragCoefficient = 1.05f;
	FrontalAreaM2 = 0.015f;

	// ---- Endurance: minutes, not half an hour. This is a weapon launched at a known target, not a
	// scout, and flying it hard is what burns the pack.
	Battery = CreateDefaultSubobject<UForgeDroneBatteryComponent>(TEXT("Battery"));
	Battery->CapacityWh = 28.f;
	Battery->AvionicsLoadW = 8.f;
	Battery->MaxPropulsionLoadW = 1200.f;
	Battery->LowChargeThreshold = 0.2f;
	Battery->CriticalChargeThreshold = 0.08f;

	// ---- Link: shorter ranged, and it keeps going if the signal drops. A drone already committed to
	// a run should finish it rather than turning around.
	Link = CreateDefaultSubobject<UForgeDroneLinkComponent>(TEXT("ControlLink"));
	Link->MaxRangeM = 5000.f;
	Link->LinkLossBehavior = EForgeDroneLinkLossBehavior::Continue;

	// ---- Warhead: armed in flight, never on the ground next to the operator.
	Warhead = CreateDefaultSubobject<UForgeDroneWarheadComponent>(TEXT("Warhead"));
	Warhead->ArmDelaySeconds = 2.f;
	Warhead->MinArmDistanceM = 30.f;
	Warhead->MinImpactSpeedMS = 6.f;
	Warhead->bDestroyOwnerOnDetonate = true;
}

void AForgeFPVKamikazeQuad::BeginPlay()
{
	if (Core)
	{
		Core->SetMassOverrideInKg(NAME_None, AirframeMassKg, true);
	}

	Super::BeginPlay();

	if (Link)
	{
		Link->SetHomeLocation(GetActorLocation());
	}
	if (Warhead)
	{
		// Arming distance is measured from here, so it can never detonate on the crew that launched it.
		Warhead->SetLaunchLocation(GetActorLocation());
	}
}

void AForgeFPVKamikazeQuad::ArmWarhead()
{
	if (Warhead)
	{
		// A request, not a grant: the warhead enforces its own delay and distance before going live.
		Warhead->RequestArm();
	}
}
