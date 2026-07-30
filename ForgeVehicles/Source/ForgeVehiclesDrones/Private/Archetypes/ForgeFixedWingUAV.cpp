// Copyright Impact-Forge. Fixed-wing UAV implementation.

#include "Archetypes/ForgeFixedWingUAV.h"

#include "Components/StaticMeshComponent.h"
#include "ForgeVehiclesDrones.h"
#include "Net/UnrealNetwork.h"
#include "Systems/ForgeDroneAutopilotComponent.h"
#include "Systems/ForgeDroneBatteryComponent.h"
#include "Systems/ForgeDroneLinkComponent.h"

AForgeFixedWingUAV::AForgeFixedWingUAV(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A 2 kg airframe has no cargo hold, no one to run over and nobody to board it.
	// Suppressing these keeps a hand-launched drone from carrying a crewed vehicle's baggage.
	ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("VehicleInventory"));
	ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("RunOverComponent"));
	ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("OccupantExitPoint"));

	ApplySmallAirframeTuning();
}

void AForgeFixedWingUAV::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AForgeFixedWingUAV, bLaunched);
}

void AForgeFixedWingUAV::ApplySmallAirframeTuning()
{
	// The inherited defaults describe a multi-tonne light transport. The coefficients are scale-free,
	// but anything quoted in absolute newtons or area has to come down by three orders of magnitude.
	WingArea = 0.35f;          // m^2, a ~1.8 m span wing
	AspectRatio = 8.f;         // high aspect ratio, as loiter airframes are
	ParasiticDrag = 0.04f;     // proportionally draggier than a clean aeroplane
	OswaldEfficiency = 0.85f;

	// Newtons of side force per m/s of sideslip. The inherited 5000 would pin a 2 kg airframe rigid.
	SlipResistance = 50.f;

	// Full control authority arrives at a far lower airspeed on a small wing.
	ControlAuthoritySpeed = 12.f;

	// Small UAVs are far more agile in roll and pitch than an airliner.
	PitchRate = 180.f;
	RollRate = 320.f;
	YawRate = 90.f;

	StallAngle = 14.f;
	BaseLiftCoefficient = 0.25f;
}

void AForgeFixedWingUAV::BeginPlay()
{
	// Mass must be overridden before the aero model samples it: a small collision volume's computed
	// mass bears no relation to the real airframe.
	if (UStaticMeshComponent* Body = GetCore())
	{
		Body->SetMassOverrideInKg(NAME_None, AirframeMassKg, true);
	}

	Super::BeginPlay();

	// Launch point doubles as the return-to-home point for link-loss failsafes.
	if (UForgeDroneLinkComponent* Link = FindComponentByClass<UForgeDroneLinkComponent>())
	{
		Link->SetHomeLocation(GetActorLocation());
	}
	if (UForgeDroneAutopilotComponent* Autopilot = FindComponentByClass<UForgeDroneAutopilotComponent>())
	{
		Autopilot->SetHomeLocation(GetActorLocation());
	}
}

void AForgeFixedWingUAV::Launch()
{
	if (!HasAuthority() || bLaunched)
	{
		return;
	}

	UStaticMeshComponent* Body = GetCore();
	if (!Body)
	{
		return;
	}

	bLaunched = true;

	// Spin the motor up first: the aircraft has to accelerate away from the launch, or it decays
	// straight into a stall.
	StartEngine();
	SetThrottleInput(LaunchThrottle);

	// A hand or bungee launch is an impulse, not a takeoff roll - there is no undercarriage and no
	// runway. Pitch it up slightly so it climbs away rather than mushing into the ground.
	const FRotator LaunchRotation = GetActorRotation() + FRotator(LaunchPitchDeg, 0.f, 0.f);
	const FVector LaunchVelocity = LaunchRotation.Vector() * LaunchSpeedMS * 100.f;
	Body->SetPhysicsLinearVelocity(LaunchVelocity);

	if (bOrbitAfterLaunch)
	{
		if (UForgeDroneAutopilotComponent* Autopilot = FindComponentByClass<UForgeDroneAutopilotComponent>())
		{
			// Loitering unattended over the launch point is the whole point of this class of drone.
			FVector OrbitCentre = GetActorLocation();
			OrbitCentre.Z += PostLaunchOrbitAltitudeM * 100.f;
			Autopilot->SetOrbit(OrbitCentre, PostLaunchOrbitRadiusM, /*SpeedMS*/ 16.f);
			Autopilot->SetMode(EForgeDroneAutopilotMode::Orbit);
		}
	}

	OnLaunched.Broadcast();
	UE_LOG(LogForgeDrones, Verbose, TEXT("%s launched at %.1f m/s."), *GetName(), LaunchSpeedMS);
}
