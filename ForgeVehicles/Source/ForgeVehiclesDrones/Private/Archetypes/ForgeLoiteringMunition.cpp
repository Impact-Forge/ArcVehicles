// Copyright Impact-Forge. Loitering munition implementation.

#include "Archetypes/ForgeLoiteringMunition.h"

#include "ForgeVehiclesDrones.h"
#include "Net/UnrealNetwork.h"
#include "Payloads/ForgeDroneWarheadComponent.h"
#include "Systems/ForgeDroneAutopilotComponent.h"
#include "Systems/ForgeDroneLinkComponent.h"

AForgeLoiteringMunition::AForgeLoiteringMunition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Warhead = CreateDefaultSubobject<UForgeDroneWarheadComponent>(TEXT("Warhead"));

	AirframeMassKg = 2.5f;
	LaunchSpeedMS = 20.f;

	// Loitering is what it does between launch and commitment, so it starts orbiting on its own.
	bOrbitAfterLaunch = true;
	PostLaunchOrbitRadiusM = 150.f;
	PostLaunchOrbitAltitudeM = 120.f;
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

	// A munition that loses its link mid-loiter should keep flying and come home rather than hold
	// station as a hovering explosive; if it has already committed, Continue is set at that point.
	if (UForgeDroneLinkComponent* Link = FindComponentByClass<UForgeDroneLinkComponent>())
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

	if (UForgeDroneAutopilotComponent* Autopilot = FindComponentByClass<UForgeDroneAutopilotComponent>())
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
	if (UForgeDroneLinkComponent* Link = FindComponentByClass<UForgeDroneLinkComponent>())
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

	if (UForgeDroneAutopilotComponent* Autopilot = FindComponentByClass<UForgeDroneAutopilotComponent>())
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

	if (UForgeDroneAutopilotComponent* Autopilot = FindComponentByClass<UForgeDroneAutopilotComponent>())
	{
		FVector OrbitCentre = GetActorLocation();
		OrbitCentre.Z += PostLaunchOrbitAltitudeM * 100.f;
		Autopilot->SetOrbit(OrbitCentre, PostLaunchOrbitRadiusM, /*SpeedMS*/ 20.f);
		Autopilot->SetMode(EForgeDroneAutopilotMode::Orbit);
	}

	if (UForgeDroneLinkComponent* Link = FindComponentByClass<UForgeDroneLinkComponent>())
	{
		Link->LinkLossBehavior = EForgeDroneLinkLossBehavior::ReturnToHome;
	}

	bCommitted = false;
	return true;
}
