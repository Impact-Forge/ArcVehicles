// Copyright Impact-Forge. Loitering munition implementation.

#include "Archetypes/ForgeLoiteringMunition.h"

#include "ForgeVehiclesDrones.h"
#include "Net/UnrealNetwork.h"
#include "Payloads/ForgeDroneWarheadComponent.h"
#include "Systems/ForgeDroneAutopilotComponent.h"
#include "Systems/ForgeDroneBatteryComponent.h"
#include "Systems/ForgeDroneLinkComponent.h"

AForgeLoiteringMunition::AForgeLoiteringMunition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Re-run the tuning. The base constructor's call to this virtual dispatched to the base override,
	// as a virtual call from a constructor always does, so our own version has not run yet. It only
	// assigns tunables, so calling it twice is harmless - and skipping it silently loses the dive
	// authority this airframe depends on.
	ApplySmallAirframeTuning();

	Warhead = CreateDefaultSubobject<UForgeDroneWarheadComponent>(TEXT("Warhead"));
	Warhead->ArmDelaySeconds = 3.f;
	Warhead->MinArmDistanceM = 50.f;
	Warhead->bDestroyOwnerOnDetonate = true;

	AirframeMassKg = 2.5f;
	LaunchSpeedMS = 20.f;

	// Loitering is what it does between launch and commitment, so it starts orbiting on its own.
	bOrbitAfterLaunch = true;
	PostLaunchOrbitRadiusM = 150.f;
	PostLaunchOrbitAltitudeM = 120.f;

	// ---- Endurance: around twenty minutes, not an hour. It is a quarter heavier than the scout on the
	// same wing, and induced drag goes with the square of weight, so it works appreciably harder to
	// stay up; it is also spent at the end of the sortie either way, so there is no reason to carry a
	// pack sized to bring it home. At a nominal 0.5 cruise throttle: 120 W of motor plus 14 W of
	// avionics against 50 Wh.
	if (Battery)
	{
		Battery->CapacityWh = 50.f;
		Battery->AvionicsLoadW = 14.f;        // avionics plus a live seeker and fuze
		Battery->MaxPropulsionLoadW = 240.f;
		Battery->LowChargeThreshold = 0.2f;   // it is not coming back, so warn later than the scout
	}

	// Slightly shorter link than the pure scout: it is used closer in, against a target already found.
	if (Link)
	{
		Link->MaxRangeM = 10000.f;
	}
}

void AForgeLoiteringMunition::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AForgeLoiteringMunition, bCommitted);
}

void AForgeLoiteringMunition::ApplySmallAirframeTuning()
{
	Super::ApplySmallAirframeTuning();

	// A terminal dive is flown well past cruise speed, and the aircraft has to stay steerable all the
	// way in. Keeping a floor under control authority means it never goes numb during the run.
	MinControlAuthority = 0.3f;

	// Slightly lower aspect ratio and more drag than a pure loiter airframe: it carries a warhead.
	AspectRatio = 7.f;
	ParasiticDrag = 0.05f;
	PitchRate = 220.f;
}

void AForgeLoiteringMunition::BeginPlay()
{
	Super::BeginPlay();

	if (Warhead)
	{
		// The launch point is where arming distance is measured from, so the munition can never
		// detonate on the crew that launched it.
		Warhead->SetLaunchLocation(GetActorLocation());
	}

	// A munition that loses its link mid-loiter should come home rather than circle unattended with a
	// warhead aboard; if it has already committed, Continue is set at that point.
	if (Link)
	{
		Link->LinkLossBehavior = EForgeDroneLinkLossBehavior::ReturnToHome;
	}
}

void AForgeLoiteringMunition::CommitToTarget(const FVector& TargetLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Autopilot)
	{
		Autopilot->SetDiveTarget(TargetLocation);
		Autopilot->SetMode(EForgeDroneAutopilotMode::TerminalDive);
	}

	if (Warhead)
	{
		// Arming is requested, not granted: the warhead still enforces its own delay and minimum
		// distance before the fuze goes live.
		Warhead->RequestArm();
	}

	// Once committed, losing the link must not turn the aircraft around - the run continues.
	if (Link)
	{
		Link->LinkLossBehavior = EForgeDroneLinkLossBehavior::Continue;
	}

	if (!bCommitted)
	{
		bCommitted = true;
		OnCommitted.Broadcast();
		UE_LOG(LogForgeDrones, Verbose, TEXT("%s committed to a target."), *GetName());
	}
}

void AForgeLoiteringMunition::CommitToTargetActor(AActor* TargetActor)
{
	if (!HasAuthority() || !TargetActor)
	{
		return;
	}

	if (Autopilot)
	{
		Autopilot->SetDiveTargetActor(TargetActor);
	}
	CommitToTarget(TargetActor->GetActorLocation());
}

bool AForgeLoiteringMunition::AbortAttack()
{
	if (!HasAuthority() || !bCommitted)
	{
		return false;
	}

	// Only abortable while the warhead is still safe. Past that the aircraft is committed - which is
	// the cost of pressing an attack, and why the decision matters.
	if (Warhead && Warhead->IsArmed())
	{
		return false;
	}

	if (Warhead)
	{
		Warhead->Safe();
	}

	if (Autopilot)
	{
		FVector OrbitCentre = GetActorLocation();
		OrbitCentre.Z += PostLaunchOrbitAltitudeM * 100.f;
		Autopilot->SetOrbit(OrbitCentre, PostLaunchOrbitRadiusM, /*SpeedMS*/ 20.f);
		Autopilot->SetMode(EForgeDroneAutopilotMode::Orbit);
	}

	if (Link)
	{
		Link->LinkLossBehavior = EForgeDroneLinkLossBehavior::ReturnToHome;
	}

	bCommitted = false;
	return true;
}
