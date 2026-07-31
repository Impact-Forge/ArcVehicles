// Copyright Impact-Forge. Drone autopilot implementation.

#include "Systems/ForgeDroneAutopilotComponent.h"

#include "Flight/ForgeMultirotorVehicle.h"
#include "ForgeVehicle.h"
#include "ForgeVehiclesDrones.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"

namespace
{
	/* Returns the owner's movement interface, or an empty wrapper. */
	TScriptInterface<IForgeVehicleMovementInterface> GetMovement(AActor* Owner)
	{
		if (AForgeVehicle* Vehicle = Cast<AForgeVehicle>(Owner))
		{
			return Vehicle->GetVehicleMovementInterface();
		}
		return TScriptInterface<IForgeVehicleMovementInterface>();
	}
}

UForgeDroneAutopilotComponent::UForgeDroneAutopilotComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Position loops work in metres of error and output normalised stick demands, so gains are small.
	ForwardPID.Kp = 0.06f;
	ForwardPID.Kd = 0.10f;
	ForwardPID.OutputMax = 1.f;
	LateralPID = ForwardPID;

	AltitudePID.Kp = 0.25f;
	AltitudePID.Kd = 0.15f;
	AltitudePID.Ki = 0.02f;
	AltitudePID.MaxIntegral = 10.f;
	AltitudePID.OutputMax = 1.f;

	// Heading works in degrees of error.
	HeadingPID.Kp = 0.03f;
	HeadingPID.Kd = 0.01f;
	HeadingPID.OutputMax = 1.f;

	SpeedPID.Kp = 0.12f;
	SpeedPID.Ki = 0.03f;
	SpeedPID.MaxIntegral = 8.f;
	SpeedPID.OutputMax = 1.f;
}

void UForgeDroneAutopilotComponent::BeginPlay()
{
	Super::BeginPlay();

	// Which control strategy this airframe needs. A multirotor translates by tilting; anything else is
	// assumed to fly like an aeroplane.
	bMultirotorProfile = GetOwner() && GetOwner()->IsA<AForgeMultirotorVehicle>();

	if (GetOwner())
	{
		HomeLocation = GetOwner()->GetActorLocation();
		TargetLocation = HomeLocation;
	}

	// The autopilot flies the aircraft, so it only runs where the physics does.
	SetComponentTickEnabled(GetOwner() && GetOwner()->HasAuthority());
}

void UForgeDroneAutopilotComponent::Activate(bool bReset)
{
	Super::Activate(bReset);
	SetActiveFlag(Mode != EForgeDroneAutopilotMode::Manual);
}

void UForgeDroneAutopilotComponent::Deactivate()
{
	Super::Deactivate();
	SetActiveFlag(Mode != EForgeDroneAutopilotMode::Manual);
}

void UForgeDroneAutopilotComponent::SetMode(const EForgeDroneAutopilotMode NewMode)
{
	bEngagedByFailsafe = false;

	// A wing cannot hold a point: it has to keep flying to stay up. For anything that is not a
	// multirotor, holding station means circling the spot, so translate the request rather than
	// commanding an attitude the aeroplane physically cannot sustain. This covers both the link-loss
	// failsafe and the hold that ReturnToHome settles into on arrival.
	if (NewMode == EForgeDroneAutopilotMode::PositionHold && !bMultirotorProfile && GetOwner())
	{
		SetOrbit(GetOwner()->GetActorLocation(), OrbitRadiusM, CruiseSpeedMS, bOrbitClockwise);
		SetMode(EForgeDroneAutopilotMode::Orbit);
		return;
	}

	if (Mode == NewMode)
	{
		return;
	}

	Mode = NewMode;
	bArrivalReported = false;
	ResetLoops();

	if (Mode == EForgeDroneAutopilotMode::Manual)
	{
		ReleaseControls();
	}
	else if (GetOwner())
	{
		// Modes that hold a point start from wherever the aircraft is, so engaging never yanks it.
		if (Mode == EForgeDroneAutopilotMode::PositionHold || Mode == EForgeDroneAutopilotMode::AltitudeHold)
		{
			TargetLocation = GetOwner()->GetActorLocation();
		}
	}

	OnModeChanged.Broadcast(Mode);
}

void UForgeDroneAutopilotComponent::SetModeFromFailsafe(const EForgeDroneAutopilotMode NewMode)
{
	SetMode(NewMode);
	// Set after SetMode, which clears the flag for pilot-commanded changes.
	bEngagedByFailsafe = true;
}

void UForgeDroneAutopilotComponent::SetOrbit(const FVector& InCentre, const float InRadiusM, const float InSpeedMS, const bool bInClockwise)
{
	OrbitCentre = InCentre;
	OrbitRadiusM = FMath::Max(InRadiusM, 1.f);
	OrbitSpeedMS = FMath::Max(InSpeedMS, 1.f);
	bOrbitClockwise = bInClockwise;

	// Start the circle at the aircraft's current bearing from the centre, so it joins the orbit from
	// wherever it happens to be instead of cutting across to a fixed entry point.
	if (GetOwner())
	{
		const FVector Offset = GetOwner()->GetActorLocation() - OrbitCentre;
		OrbitPhase = FMath::Atan2(Offset.Y, Offset.X);
	}
}

void UForgeDroneAutopilotComponent::SetDiveTarget(const FVector& InTarget)
{
	DiveTargetActor = nullptr;
	TargetLocation = InTarget;
}

void UForgeDroneAutopilotComponent::SetDiveTargetActor(AActor* InTargetActor)
{
	DiveTargetActor = InTargetActor;
	if (InTargetActor)
	{
		TargetLocation = InTargetActor->GetActorLocation();
	}
}

void UForgeDroneAutopilotComponent::ResetLoops()
{
	ForwardPID.Reset();
	LateralPID.Reset();
	AltitudePID.Reset();
	HeadingPID.Reset();
	SpeedPID.Reset();
}

void UForgeDroneAutopilotComponent::ReleaseControls()
{
	if (TScriptInterface<IForgeVehicleMovementInterface> Movement = GetMovement(GetOwner()))
	{
		Movement->SetThrottleInput(0.f);
		Movement->SetSteeringInput(0.f);
		Movement->SetVerticalInput(0.f);
		Movement->SetRollAxisInput(0.f);
		Movement->SetYawAxisInput(0.f);
	}
}

void UForgeDroneAutopilotComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Mode == EForgeDroneAutopilotMode::Manual || !GetOwner() || DeltaTime <= 0.f)
	{
		return;
	}

	UpdateTarget(DeltaTime);

	if (bMultirotorProfile)
	{
		FlyMultirotor(DeltaTime);
	}
	else
	{
		FlyFixedWing(DeltaTime);
	}
}

void UForgeDroneAutopilotComponent::UpdateTarget(const float DeltaTime)
{
	const FVector CurrentLocation = GetOwner()->GetActorLocation();

	switch (Mode)
	{
	case EForgeDroneAutopilotMode::AltitudeHold:
		// Only the vertical component matters; horizontal is left to the pilot.
		break;

	case EForgeDroneAutopilotMode::PositionHold:
		// TargetLocation was captured when the mode engaged.
		break;

	case EForgeDroneAutopilotMode::Orbit:
	{
		// Advance around the circle at the commanded ground speed: omega = v / r.
		const float AngularSpeed = OrbitSpeedMS / FMath::Max(OrbitRadiusM, 1.f);
		OrbitPhase += (bOrbitClockwise ? -1.f : 1.f) * AngularSpeed * DeltaTime;

		const float RadiusCm = OrbitRadiusM * 100.f;
		TargetLocation = OrbitCentre + FVector(FMath::Cos(OrbitPhase) * RadiusCm, FMath::Sin(OrbitPhase) * RadiusCm, 0.f);
		// Hold the altitude the orbit was set up at.
		TargetLocation.Z = OrbitCentre.Z;
		break;
	}

	case EForgeDroneAutopilotMode::ReturnToHome:
	{
		// Climb to the transit altitude first, then run home at that height, so the aircraft does not
		// try to fly through whatever is between it and the launch point.
		TargetLocation = HomeLocation;
		TargetLocation.Z = HomeLocation.Z + ReturnAltitudeM * 100.f;

		const float HorizontalDistanceM = FVector::Dist2D(CurrentLocation, HomeLocation) / 100.f;
		if (!bArrivalReported && HorizontalDistanceM <= ArrivalToleranceM)
		{
			bArrivalReported = true;
			OnArrived.Broadcast();
			// Arriving home ends the transit; hold station rather than circling the launch point.
			SetMode(EForgeDroneAutopilotMode::PositionHold);
		}
		break;
	}

	case EForgeDroneAutopilotMode::TerminalDive:
		if (const AActor* Target = DiveTargetActor.Get())
		{
			TargetLocation = Target->GetActorLocation();
		}
		break;

	default:
		break;
	}
}

void UForgeDroneAutopilotComponent::FlyMultirotor(const float DeltaTime)
{
	TScriptInterface<IForgeVehicleMovementInterface> Movement = GetMovement(GetOwner());
	if (!Movement)
	{
		return;
	}

	const FTransform ActorTransform = GetOwner()->GetActorTransform();
	const FVector ErrorWorld = TargetLocation - ActorTransform.GetLocation();

	// Altitude is its own channel on a multirotor: the collective.
	const float AltitudeErrorM = ErrorWorld.Z / 100.f;
	const float Collective = AltitudePID.Update(AltitudeErrorM, DeltaTime);
	Movement->SetVerticalInput(FMath::Clamp(Collective, -1.f, 1.f));

	if (Mode == EForgeDroneAutopilotMode::AltitudeHold)
	{
		// Horizontal control stays with the pilot.
		return;
	}

	// Horizontal error expressed in the aircraft's own frame, so "forward" means forward for it.
	const FVector LocalError = ActorTransform.InverseTransformVectorNoScale(FVector(ErrorWorld.X, ErrorWorld.Y, 0.f));
	const float ForwardErrorM = LocalError.X / 100.f;
	const float LateralErrorM = LocalError.Y / 100.f;

	// Translation comes from tilting, and the tilt limit is what caps the autopilot's speed.
	const float TiltScale = FMath::Clamp(MaxAutoTiltDeg / 45.f, 0.f, 1.f);
	const bool bDiving = Mode == EForgeDroneAutopilotMode::TerminalDive;
	const float AuthorityScale = bDiving ? 1.f : TiltScale;

	Movement->SetThrottleInput(FMath::Clamp(ForwardPID.Update(ForwardErrorM, DeltaTime), -1.f, 1.f) * AuthorityScale);
	Movement->SetRollAxisInput(FMath::Clamp(LateralPID.Update(LateralErrorM, DeltaTime), -1.f, 1.f) * AuthorityScale);

	// Point the nose at wherever it is going, which also aims a fixed camera or warhead.
	const FVector ToTarget2D = FVector(ErrorWorld.X, ErrorWorld.Y, 0.f);
	if (!ToTarget2D.IsNearlyZero())
	{
		const float DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget2D.Y, ToTarget2D.X));
		const float YawErrorDeg = FRotator::NormalizeAxis(DesiredYaw - ActorTransform.Rotator().Yaw);
		Movement->SetYawAxisInput(FMath::Clamp(HeadingPID.Update(YawErrorDeg, DeltaTime), -1.f, 1.f));
	}

	if (bDiving)
	{
		// Committed: hold the nose down on the target and stop trying to keep altitude.
		const float DiveErrorM = -FMath::Abs(AltitudeErrorM);
		Movement->SetVerticalInput(FMath::Clamp(AltitudePID.Update(DiveErrorM, DeltaTime), -1.f, 0.f));
	}
}

void UForgeDroneAutopilotComponent::FlyFixedWing(const float DeltaTime)
{
	TScriptInterface<IForgeVehicleMovementInterface> Movement = GetMovement(GetOwner());
	if (!Movement)
	{
		return;
	}

	const FTransform ActorTransform = GetOwner()->GetActorTransform();
	const FVector ErrorWorld = TargetLocation - ActorTransform.GetLocation();
	const FRotator CurrentRotation = ActorTransform.Rotator();

	// Bank to turn: heading error drives roll, which is how an aeroplane actually changes direction.
	// Note the fixed-wing vehicle maps SetSteeringInput onto its ailerons.
	const FVector ToTarget2D = FVector(ErrorWorld.X, ErrorWorld.Y, 0.f);
	if (!ToTarget2D.IsNearlyZero())
	{
		const float DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget2D.Y, ToTarget2D.X));
		const float YawErrorDeg = FRotator::NormalizeAxis(DesiredYaw - CurrentRotation.Yaw);

		const float BankScale = FMath::Clamp(MaxAutoBankDeg / 90.f, 0.f, 1.f);
		Movement->SetSteeringInput(FMath::Clamp(HeadingPID.Update(YawErrorDeg, DeltaTime), -1.f, 1.f) * BankScale);
	}

	if (Mode == EForgeDroneAutopilotMode::TerminalDive)
	{
		// Proportional navigation on the line of sight: pitch onto the target and go to full power.
		const float DistanceHorizontal = FMath::Max(FVector::Dist2D(TargetLocation, ActorTransform.GetLocation()), 1.f);
		const float DesiredPitchDeg = FMath::RadiansToDegrees(FMath::Atan2(ErrorWorld.Z, DistanceHorizontal));
		const float PitchErrorDeg = FRotator::NormalizeAxis(DesiredPitchDeg - CurrentRotation.Pitch);

		Movement->SetVerticalInput(FMath::Clamp(PitchErrorDeg * 0.05f, -1.f, 1.f));
		Movement->SetThrottleInput(1.f);
		return;
	}

	// Altitude via pitch, speed via throttle - the standard split for a cruising aeroplane.
	const float AltitudeErrorM = ErrorWorld.Z / 100.f;
	Movement->SetVerticalInput(FMath::Clamp(AltitudePID.Update(AltitudeErrorM, DeltaTime), -1.f, 1.f));

	const float TargetSpeedMS = (Mode == EForgeDroneAutopilotMode::Orbit) ? OrbitSpeedMS : CruiseSpeedMS;
	const float CurrentSpeedMS = GetOwner()->GetVelocity().Size() / 100.f;
	const float ThrottleCorrection = SpeedPID.Update(TargetSpeedMS - CurrentSpeedMS, DeltaTime);

	// Fixed-wing throttle is [0, 1], so bias around a cruise setting rather than swinging to idle.
	Movement->SetThrottleInput(FMath::Clamp(0.6f + ThrottleCorrection, 0.f, 1.f));
}
