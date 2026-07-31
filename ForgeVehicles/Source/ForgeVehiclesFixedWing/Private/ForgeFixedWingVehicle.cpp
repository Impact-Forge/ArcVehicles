// K2 FixedWing Physics

#include "ForgeFixedWingVehicle.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

static const float K2FW_GRAVITY = 9.81f; // m/s^2

AForgeFixedWingVehicle::AForgeFixedWingVehicle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// Replication is opt-in via ReplicationMethod, but we register as a replicated actor and disable the
	// engine's built-in movement replication because state is synced manually (see ServerStateSync).
	bReplicates = true;
	AActor::SetReplicateMovement(false);

	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	RootComponent = Core;
}

void AForgeFixedWingVehicle::BeginPlay()
{
	Super::BeginPlay();

	if (Core)
	{
		Core->SetSimulatePhysics(true);
		Core->SetAngularDamping(RotationalDamping);
		Core->SetLinearDamping(LinearDamping);
		Mass = Core->GetMass();
	}

	TiltAlpha = bVTOLCapable ? FMath::Clamp(InitialTiltAlpha, 0.f, 1.f) : 0.f;
	TiltTarget = TiltAlpha;

	// Collect the powerplants.
	TArray<UActorComponent*> engineComps;
	GetComponents(UForgeAeroEngineComponent::StaticClass(), engineComps);
	for (UActorComponent* c : engineComps)
	{
		if (UForgeAeroEngineComponent* e = Cast<UForgeAeroEngineComponent>(c))
		{
			Engines.Add(e);
		}
	}

	// Collect the (cosmetic) control surfaces.
	TArray<UActorComponent*> surfaceComps;
	GetComponents(UForgeControlSurfaceComponent::StaticClass(), surfaceComps);
	for (UActorComponent* c : surfaceComps)
	{
		if (UForgeControlSurfaceComponent* s = Cast<UForgeControlSurfaceComponent>(c))
		{
			ControlSurfaces.Add(s);
		}
	}
}

void AForgeFixedWingVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AForgeFixedWingVehicle, ServerState);
}

void AForgeFixedWingVehicle::UpdatePhysics(float DeltaTime)
{
	if (!Core) { return; }

	Core->WakeAllRigidBodies();

	// Swing the tiltrotor nacelles toward their target.
	TiltAlpha = bVTOLCapable ? FMath::FInterpTo(TiltAlpha, TiltTarget, DeltaTime, TiltRate) : 0.f;

	const FTransform xform = GetActorTransform();
	const FVector f = Core->GetForwardVector();
	const FVector r = Core->GetRightVector();
	const FVector u = Core->GetUpVector();

	const FVector worldVel = GetVelocity();
	const FVector localVel = UKismetMathLibrary::InverseTransformDirection(xform, worldVel);

	ForwardSpeed = localVel.X / 100.f; // m/s, signed
	Airspeed = worldVel.Size() / 100.f; // m/s, total

	if (FMath::Abs(localVel.X) > 1.f)
	{
		AngleOfAttack = FMath::RadiansToDegrees(FMath::Atan2(-localVel.Z, localVel.X));
		SideSlip = FMath::RadiansToDegrees(FMath::Atan2(localVel.Y, localVel.X));
	}
	else
	{
		AngleOfAttack = 0.f;
		SideSlip = 0.f;
	}

	// Atmosphere: density thins with altitude (optional), feeding lift, drag and air-breathing thrust.
	AltitudeMeters = (GetActorLocation().Z - SeaLevelZ) / 100.f;
	CurrentAirDensity = bUseAltitudeDensity
		? AirDensity * FMath::Exp(-FMath::Max(AltitudeMeters, 0.f) / DensityScaleHeight)
		: AirDensity;
	MachNumber = (SpeedOfSound > KINDA_SMALL_NUMBER) ? (Airspeed / SpeedOfSound) : 0.f;

	const float invertP = bInvertPitch ? -1.f : 1.f;
	const float invertR = bInvertRoll ? -1.f : 1.f;
	const float invertY = bInvertYaw ? -1.f : 1.f;

	// Dynamic pressure from forward airspeed only - the wing needs air flowing over it from the front.
	const float vForward = FMath::Max(ForwardSpeed, 0.f);
	const float q = 0.5f * CurrentAirDensity * vForward * vForward; // Pa (N/m^2)

	// ---- LIFT ------------------------------------------------------------------------------------
	float CL = BaseLiftCoefficient + LiftCurveSlope * AngleOfAttack;
	if (bUseFlaps)
	{
		CL += FlapsLiftCoefficient * FlapsInput;
	}

	const float absAoA = FMath::Abs(AngleOfAttack);
	bStalled = absAoA > StallAngle;
	if (bStalled)
	{
		const float stallFactor = FMath::GetMappedRangeValueClamped(
			FVector2D(StallAngle, StallAngle + StallSharpness),
			FVector2D(1.f, 0.1f), absAoA);
		CL *= stallFactor;
	}

	LiftForce = bUseLift ? (q * WingArea * CL) : 0.f;
	if (bUseLift)
	{
		Core->AddForce(u * LiftForce * 100.f); // N -> Unreal force units
	}

	// ---- DRAG (parasitic + induced + flaps) ------------------------------------------------------
	// Induced drag comes from producing lift, so it only applies when the wing is generating lift.
	const float inducedCL = bUseLift ? CL : 0.f;
	float CD = ParasiticDrag + (inducedCL * inducedCL) / (PI * AspectRatio * OswaldEfficiency);
	if (bUseFlaps)
	{
		CD += FlapsDragCoefficient * FlapsInput;
	}
	// Transonic drag rise: a bump centred on Mach 1 (the sound barrier).
	if (bUseMachDrag)
	{
		const float bump = MachDragPeak * FMath::Exp(-FMath::Square((MachNumber - 1.f) / MachDragWidth));
		CD *= (1.f + bump);
	}
	DragForce = q * WingArea * CD;
	const FVector velDir = worldVel.GetSafeNormal();
	Core->AddForce(-velDir * DragForce * 100.f);

	// ---- Side force (resist slipping sideways through the air) ------------------------------------
	{
		const float sideForceN = -(localVel.Y / 100.f) * SlipResistance;
		Core->AddForce(r * sideForceN * 100.f);
	}

	// ---- CONTROL TORQUES -------------------------------------------------------------------------
	// Aerodynamic control authority grows with forward airspeed; hover authority comes from the tilted
	// rotors (reaction / differential thrust) so attitude control is available at zero airspeed in hover.
	const float aeroAuth = FMath::GetMappedRangeValueClamped(FVector2D(0.f, ControlAuthoritySpeed), FVector2D(0.f, 1.f), FMath::Abs(ForwardSpeed));
	const float hoverAuth = TiltAlpha;
	float ctrl = FMath::Max3(aeroAuth, hoverAuth, MinControlAuthority);
	ctrl = FMath::Min(ctrl, 1.5f);

	Core->AddTorqueInDegrees(r * invertP * PitchInput * PitchRate * ctrl, NAME_None, true);
	Core->AddTorqueInDegrees(-f * invertR * RollInput * RollRate * ctrl, NAME_None, true);
	Core->AddTorqueInDegrees(-u * invertY * YawInput * YawRate * ctrl, NAME_None, true);

	// ---- AERODYNAMIC STABILITY (point the nose into the wind; fades in with airspeed) -------------
	if (bUseAerodynamicStability)
	{
		Core->AddTorqueInDegrees(r * invertP * AngleOfAttack * PitchStability * aeroAuth, NAME_None, true);
		Core->AddTorqueInDegrees(-u * invertY * SideSlip * YawStability * aeroAuth, NAME_None, true);
		Core->AddTorqueInDegrees(-f * invertR * SideSlip * DihedralEffect * aeroAuth, NAME_None, true);
	}

	// ---- AUTO LEVEL (optional arcade aid; only with airflow, fades out under pilot input) ---------
	if (bAutoLevel)
	{
		const FRotator a = GetActorRotation();
		const float pitchLevel = FMath::Lerp(AutoLevelStrength, 0.f, FMath::Abs(PitchInput)) * aeroAuth;
		Core->AddTorqueInDegrees(r * invertP * a.Pitch * pitchLevel, NAME_None, true);
		const float rollLevel = FMath::Lerp(AutoLevelStrength, 0.f, FMath::Abs(RollInput)) * aeroAuth;
		Core->AddTorqueInDegrees(f * invertR * a.Roll * rollLevel, NAME_None, true);
	}

	// ---- HOVER STABILIZATION (VTOL low-speed aid) ------------------------------------------------
	if (bVTOLCapable && bHoverStabilization && TiltAlpha > 0.01f)
	{
		const FRotator a = GetActorRotation();
		const float lvl = HoverLevelStrength * TiltAlpha;
		Core->AddTorqueInDegrees(r * invertP * a.Pitch * lvl, NAME_None, true);
		Core->AddTorqueInDegrees(f * invertR * a.Roll * lvl, NAME_None, true);

		// Damp horizontal drift fully and vertical motion partially (so throttle still controls climb).
		FVector damp = worldVel;
		damp.Z *= 0.3f;
		Core->AddForce(-damp * HoverDriftDamping * TiltAlpha);
	}

	// ---- HOVER GRAVITY ASSIST (optional) ---------------------------------------------------------
	if (bVTOLCapable && bHoverGravityAssist && TiltAlpha > 0.01f)
	{
		const float weightN = (Mass + AddedMass) * K2FW_GRAVITY;
		const float liftFrac = FMath::Clamp(LiftForce / FMath::Max(weightN, 1.f), 0.f, 1.f);
		const float assistN = weightN * TiltAlpha * (1.f - liftFrac);
		Core->AddForce(FVector::UpVector * assistN * 100.f);
	}
}

void AForgeFixedWingVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	GameThreadDeltaTime = DeltaTime;

	if (ReplicationMethod == EForgeFixedWingReplicationType::ERT_None)
	{
		UpdatePhysics(DeltaTime);
	}
	else if (ReplicationMethod == EForgeFixedWingReplicationType::ERT_K2)
	{
		if (HasAuthority())
		{
			UpdatePhysics(DeltaTime);
			ServerStateSync(true);
		}
		else
		{
			ClientStateSync(true);
		}
	}
	else if (ReplicationMethod == EForgeFixedWingReplicationType::ERT_DataOnly)
	{
		if (HasAuthority())
		{
			UpdatePhysics(DeltaTime);
			ServerStateSync(false);
		}
		else
		{
			ClientStateSync(false);
		}
	}

	// Drive the engines (thrust + spin/tilt) and control surfaces. These run on every machine so the
	// visuals (prop spin, nacelle tilt, surface deflection) are correct for remote clients too.
	const float densityRatio = (AirDensity > KINDA_SMALL_NUMBER) ? (CurrentAirDensity / AirDensity) : 1.f;

	ThrustForce = 0.f;
	for (UForgeAeroEngineComponent* engine : Engines)
	{
		if (!engine) { continue; }
		engine->UpdateEngine(GameThreadDeltaTime, Core, ThrottleInput, PitchInput, RollInput, YawInput, Airspeed, TiltAlpha, densityRatio);
		ThrustForce += engine->GetThrustNewtons();
	}

	for (UForgeControlSurfaceComponent* surface : ControlSurfaces)
	{
		if (!surface) { continue; }
		surface->UpdateSurface(GameThreadDeltaTime, PitchInput, RollInput, YawInput, FlapsInput);
	}
}

void AForgeFixedWingVehicle::ServerStateSync(bool updatePhysics)
{
	if (updatePhysics && Core)
	{
		ServerState.ServerLinearVelocity = Core->GetPhysicsLinearVelocity();
		ServerState.ServerAngularVelocity = Core->GetPhysicsAngularVelocityInDegrees();
		ServerState.ServerTransform = GetTransform();
	}

	ServerState.ServerThrottle = ThrottleInput;
	ServerState.ServerPitchInput = PitchInput;
	ServerState.ServerYawInput = YawInput;
	ServerState.ServerRollInput = RollInput;
	ServerState.ServerFlapsInput = FlapsInput;
	ServerState.ServerTiltAlpha = TiltAlpha;
	ServerState.ServerTiltTarget = TiltTarget;
}

void AForgeFixedWingVehicle::ClientStateSync(bool updatePhysics)
{
	if (updatePhysics && Core)
	{
		Core->SetPhysicsLinearVelocity(ServerState.ServerLinearVelocity);
		Core->SetPhysicsAngularVelocityInDegrees(ServerState.ServerAngularVelocity);
	}

	ThrottleInput = ServerState.ServerThrottle;
	PitchInput = ServerState.ServerPitchInput;
	YawInput = ServerState.ServerYawInput;
	RollInput = ServerState.ServerRollInput;
	FlapsInput = ServerState.ServerFlapsInput;
	TiltAlpha = ServerState.ServerTiltAlpha;
	TiltTarget = ServerState.ServerTiltTarget;
}

float AForgeFixedWingVehicle::GetControllableScalar() const
{
	return bInputEnabled ? 1.f : 0.f;
}

void AForgeFixedWingVehicle::SetAddedMassAmount(float massAmount)
{
	AddedMass = massAmount;
}

// ---- Throttle -----------------------------------------------------------------------------------

void AForgeFixedWingVehicle::SetThrottleInput(float value)
{
	ThrottleInput = FMath::Clamp(value, 0.f, 1.f) * GetControllableScalar();
	ServerSetThrottleInput(ThrottleInput);
}
void AForgeFixedWingVehicle::ServerSetThrottleInput_Implementation(float value)
{
	ThrottleInput = FMath::Clamp(value, 0.f, 1.f) * GetControllableScalar();
}
bool AForgeFixedWingVehicle::ServerSetThrottleInput_Validate(float value)
{
	return value >= 0.f && value <= 1.f;
}

// ---- Pitch --------------------------------------------------------------------------------------

void AForgeFixedWingVehicle::SetPitchInput(float value)
{
	PitchInput = value * GetControllableScalar();
	ServerSetPitchInput(PitchInput);
}
void AForgeFixedWingVehicle::ServerSetPitchInput_Implementation(float value)
{
	PitchInput = value * GetControllableScalar();
}
bool AForgeFixedWingVehicle::ServerSetPitchInput_Validate(float value)
{
	return FMath::Abs(value) <= 1.f;
}

// ---- Yaw ----------------------------------------------------------------------------------------

void AForgeFixedWingVehicle::SetYawInput(float value)
{
	YawInput = value * GetControllableScalar();
	ServerSetYawInput(YawInput);
}
void AForgeFixedWingVehicle::ServerSetYawInput_Implementation(float value)
{
	YawInput = value * GetControllableScalar();
}
bool AForgeFixedWingVehicle::ServerSetYawInput_Validate(float value)
{
	return FMath::Abs(value) <= 1.f;
}

// ---- Roll ---------------------------------------------------------------------------------------

void AForgeFixedWingVehicle::SetRollInput(float value)
{
	RollInput = value * GetControllableScalar();
	ServerSetRollInput(RollInput);
}
void AForgeFixedWingVehicle::ServerSetRollInput_Implementation(float value)
{
	RollInput = value * GetControllableScalar();
}
bool AForgeFixedWingVehicle::ServerSetRollInput_Validate(float value)
{
	return FMath::Abs(value) <= 1.f;
}

// ---- Flaps --------------------------------------------------------------------------------------

void AForgeFixedWingVehicle::SetFlapsInput(float value)
{
	FlapsInput = FMath::Clamp(value, 0.f, 1.f) * GetControllableScalar();
	ServerSetFlapsInput(FlapsInput);
}
void AForgeFixedWingVehicle::ServerSetFlapsInput_Implementation(float value)
{
	FlapsInput = FMath::Clamp(value, 0.f, 1.f) * GetControllableScalar();
}
bool AForgeFixedWingVehicle::ServerSetFlapsInput_Validate(float value)
{
	return value >= 0.f && value <= 1.f;
}

// ---- Controllable -------------------------------------------------------------------------------

void AForgeFixedWingVehicle::SetControllable(bool inputEnabled)
{
	bInputEnabled = inputEnabled;
	ServerSetControllable(bInputEnabled);
}
void AForgeFixedWingVehicle::ServerSetControllable_Implementation(bool inputEnabled)
{
	bInputEnabled = inputEnabled;
}
bool AForgeFixedWingVehicle::ServerSetControllable_Validate(bool inputEnabled)
{
	return true;
}

// ---- VTOL ---------------------------------------------------------------------------------------

void AForgeFixedWingVehicle::SetTiltTarget(float value)
{
	TiltTarget = FMath::Clamp(value, 0.f, 1.f);
	ServerSetTiltTarget(TiltTarget);
}
void AForgeFixedWingVehicle::ServerSetTiltTarget_Implementation(float value)
{
	TiltTarget = FMath::Clamp(value, 0.f, 1.f);
}
bool AForgeFixedWingVehicle::ServerSetTiltTarget_Validate(float value)
{
	return value >= 0.f && value <= 1.f;
}

void AForgeFixedWingVehicle::SetVTOLMode(bool bHover)
{
	SetTiltTarget(bHover ? 1.f : 0.f);
}

void AForgeFixedWingVehicle::ToggleVTOLMode()
{
	SetTiltTarget(TiltTarget < 0.5f ? 1.f : 0.f);
}

// ---- Input binding ------------------------------------------------------------------------------

void AForgeFixedWingVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// ---- Telemetry ----------------------------------------------------------------------------------

float AForgeFixedWingVehicle::GetForwardSpeedMS() { return ForwardSpeed; }
float AForgeFixedWingVehicle::GetAirspeedMS() { return Airspeed; }
float AForgeFixedWingVehicle::GetSpeedKPH() { return Airspeed * 3.6f; }
float AForgeFixedWingVehicle::GetSpeedKnots() { return Airspeed * 1.94384f; }
float AForgeFixedWingVehicle::GetMachNumber() { return MachNumber; }
float AForgeFixedWingVehicle::GetAltitudeMeters() { return AltitudeMeters; }
float AForgeFixedWingVehicle::GetCurrentAirDensity() { return CurrentAirDensity; }
float AForgeFixedWingVehicle::GetAngleOfAttack() { return AngleOfAttack; }
float AForgeFixedWingVehicle::GetSideSlip() { return SideSlip; }
float AForgeFixedWingVehicle::GetLift() { return LiftForce / K2FW_GRAVITY; }
float AForgeFixedWingVehicle::GetDrag() { return DragForce / K2FW_GRAVITY; }
float AForgeFixedWingVehicle::GetThrust() { return ThrustForce; }
bool AForgeFixedWingVehicle::IsStalled() { return bStalled; }
float AForgeFixedWingVehicle::GetThrottlePercent() { return ThrottleInput * 100.f; }
float AForgeFixedWingVehicle::GetTiltAlpha() { return TiltAlpha; }
bool AForgeFixedWingVehicle::IsVTOLEquipped() { return bVTOLCapable; }

float AForgeFixedWingVehicle::GetHoverThrottle()
{
	float totalMax = 0.f;
	for (UForgeAeroEngineComponent* engine : Engines)
	{
		if (engine)
		{
			totalMax += engine->GetMaxThrust();
		}
	}
	if (totalMax <= 0.f) { return 0.f; }
	return FMath::Clamp(((Mass + AddedMass) * K2FW_GRAVITY) / totalMax, 0.f, 1.f);
}
