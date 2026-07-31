// Copyright Impact-Forge. Multirotor flight model implementation.

#include "Flight/ForgeMultirotorVehicle.h"

#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "ForgeVehiclesDrones.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "Operator/ForgeDroneOperatorComponent.h"
#include "RotorComponent.h"
#include "Systems/ForgeWindSubsystem.h"

namespace
{
	/* Newtons to Unreal force units (cm-based). */
	constexpr float NewtonsToUnreal = 100.f;
}

AForgeMultirotorVehicle::AForgeMultirotorVehicle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	SetRootComponent(Core);

	// Every multirotor in this module is unmanned, so it always needs a way to be handed to a player and
	// a way to give them back afterwards.
	Operator = CreateDefaultSubobject<UForgeDroneOperatorComponent>(TEXT("Operator"));

	// This class synchronises its own state, so engine movement replication would be a second,
	// conflicting source of truth (same reasoning as the fixed-wing vehicle).
	SetReplicateMovement(false);

	// Rate loops: modest proportional gain with derivative damping. Integral is deliberately zero -
	// a multirotor's rate loop does not need steady-state correction and integral windup during
	// saturation is a classic source of oscillation.
	PitchRatePID.Kp = 0.0035f;
	PitchRatePID.Kd = 0.00025f;
	PitchRatePID.OutputMax = 1.f;
	RollRatePID = PitchRatePID;
	YawRatePID.Kp = 0.0045f;
	YawRatePID.Kd = 0.0002f;
	YawRatePID.OutputMax = 1.f;

	// Climb loop does want integral: it has to hold against a constant gravity bias.
	ClimbRatePID.Kp = 0.12f;
	ClimbRatePID.Kd = 0.02f;
	ClimbRatePID.Ki = 0.05f;
	ClimbRatePID.MaxIntegral = 5.f;
	ClimbRatePID.OutputMax = 1.f;
}

void AForgeMultirotorVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AForgeMultirotorVehicle, ServerState);
	DOREPLIFETIME(AForgeMultirotorVehicle, bMotorsArmed);
	DOREPLIFETIME(AForgeMultirotorVehicle, MotorEnabledMask);
}

void AForgeMultirotorVehicle::BeginPlay()
{
	Super::BeginPlay();

	EnsureMotorLayout();
	CacheRotorComponents();

	if (Core)
	{
		Core->SetSimulatePhysics(true);
		Core->SetAngularDamping(RotationalDamping);
		MassKg = Core->GetMass();
	}

	if (MassKg <= UE_SMALL_NUMBER)
	{
		MassKg = 1.f;
		UE_LOG(LogForgeDrones, Warning,
			TEXT("%s has no usable mass. Set a Mass Override (kg) on the Core body: a small drone's ")
			TEXT("auto-computed mass from collision volume is never the intended value."),
			*GetNameSafe(this));
	}

	MotorOutputs.SetNumZeroed(Motors.Num());
}

void AForgeMultirotorVehicle::EnsureMotorLayout()
{
	if (Motors.Num() > 0)
	{
		return;
	}

	// Default X-configuration quad, motors numbered clockwise from front-right with alternating
	// propeller directions so reaction torques cancel in hover.
	Motors.SetNum(4);
	const float Arm = ArmLengthCm;
	const FVector Offsets[4] = {
		FVector(Arm, Arm, 0.f),   // front right
		FVector(Arm, -Arm, 0.f),  // front left
		FVector(-Arm, -Arm, 0.f), // rear left
		FVector(-Arm, Arm, 0.f)   // rear right
	};
	const bool bClockwise[4] = { false, true, false, true };

	for (int32 Index = 0; Index < 4; ++Index)
	{
		Motors[Index].RelativeLocationCm = Offsets[Index];
		Motors[Index].bClockwise = bClockwise[Index];
		Motors[Index].MaxThrustN = DefaultMotorThrustN;
	}
}

void AForgeMultirotorVehicle::CacheRotorComponents()
{
	CachedRotors.SetNum(Motors.Num());

	TInlineComponentArray<URotorComponent*> Rotors(this);
	for (int32 Index = 0; Index < Motors.Num(); ++Index)
	{
		CachedRotors[Index] = nullptr;
		if (Motors[Index].RotorComponentName.IsNone())
		{
			continue;
		}
		for (URotorComponent* Rotor : Rotors)
		{
			if (IsValid(Rotor) && Rotor->GetFName() == Motors[Index].RotorComponentName)
			{
				CachedRotors[Index] = Rotor;
				break;
			}
		}
	}
}

void AForgeMultirotorVehicle::BuildMotorGeometry(TArray<ForgeDrone::Multirotor::FMotorGeometry>& OutGeometry) const
{
	OutGeometry.SetNum(Motors.Num());
	for (int32 Index = 0; Index < Motors.Num(); ++Index)
	{
		OutGeometry[Index].RelativeLocationCm = Motors[Index].RelativeLocationCm;
		OutGeometry[Index].bClockwise = Motors[Index].bClockwise;
		OutGeometry[Index].MaxThrustN = Motors[Index].MaxThrustN;
		OutGeometry[Index].bEnabled = Motors[Index].bEnabled;
	}
}

// ---------------------------------------------------------------- input

void AForgeMultirotorVehicle::SetThrottleStick(const float Value)
{
	ThrottleStick = FMath::Clamp(Value, -1.f, 1.f);
	if (!HasAuthority())
	{
		ServerSetSticks(ThrottleStick, PitchStick, RollStick, YawStick);
	}
}

void AForgeMultirotorVehicle::SetPitchStick(const float Value)
{
	PitchStick = FMath::Clamp(Value, -1.f, 1.f);
	if (!HasAuthority())
	{
		ServerSetSticks(ThrottleStick, PitchStick, RollStick, YawStick);
	}
}

void AForgeMultirotorVehicle::SetRollStick(const float Value)
{
	RollStick = FMath::Clamp(Value, -1.f, 1.f);
	if (!HasAuthority())
	{
		ServerSetSticks(ThrottleStick, PitchStick, RollStick, YawStick);
	}
}

void AForgeMultirotorVehicle::SetYawStick(const float Value)
{
	YawStick = FMath::Clamp(Value, -1.f, 1.f);
	if (!HasAuthority())
	{
		ServerSetSticks(ThrottleStick, PitchStick, RollStick, YawStick);
	}
}

void AForgeMultirotorVehicle::ServerSetSticks_Implementation(const float InThrottle, const float InPitch, const float InRoll, const float InYaw)
{
	// Unreliable and packed: these are continuously varying axes sent every frame, so a reliable
	// per-axis RPC would be four ordered channels of guaranteed-stale data.
	ThrottleStick = FMath::Clamp(InThrottle, -1.f, 1.f);
	PitchStick = FMath::Clamp(InPitch, -1.f, 1.f);
	RollStick = FMath::Clamp(InRoll, -1.f, 1.f);
	YawStick = FMath::Clamp(InYaw, -1.f, 1.f);
}

void AForgeMultirotorVehicle::SetFlightMode(const EForgeMultirotorFlightMode NewMode)
{
	if (FlightMode == NewMode)
	{
		return;
	}

	FlightMode = NewMode;
	PitchRatePID.Reset();
	RollRatePID.Reset();
	YawRatePID.Reset();
	ClimbRatePID.Reset();

	if (!HasAuthority())
	{
		ServerSetFlightMode(NewMode);
	}
	OnFlightModeChanged.Broadcast(NewMode);
}

void AForgeMultirotorVehicle::ServerSetFlightMode_Implementation(const EForgeMultirotorFlightMode NewMode)
{
	SetFlightMode(NewMode);
}

void AForgeMultirotorVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// The shared base binds throttle/steering/vertical; a multirotor needs a fourth axis and a
	// stabilisation-mode switch on top.
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (RollAction)
		{
			EnhancedInput->BindAction(RollAction, ETriggerEvent::Triggered, this, &AForgeMultirotorVehicle::Input_Roll);
			EnhancedInput->BindAction(RollAction, ETriggerEvent::Completed, this, &AForgeMultirotorVehicle::Input_Roll);
		}
		if (FlightModeToggleAction)
		{
			EnhancedInput->BindAction(FlightModeToggleAction, ETriggerEvent::Started, this, &AForgeMultirotorVehicle::Input_ToggleFlightMode);
		}
	}
}

void AForgeMultirotorVehicle::Input_Roll(const FInputActionValue& Value)
{
	SetRollStick(Value.Get<float>());
}

void AForgeMultirotorVehicle::Input_ToggleFlightMode(const FInputActionValue& Value)
{
	SetFlightMode(FlightMode == EForgeMultirotorFlightMode::Angle
		? EForgeMultirotorFlightMode::Acro
		: EForgeMultirotorFlightMode::Angle);
}

// ---------------------------------------------------------------- engine / motors

void AForgeMultirotorVehicle::StartEngine()
{
	bMotorsArmed = true;
}

void AForgeMultirotorVehicle::StopEngine()
{
	bMotorsArmed = false;
	for (float& Output : MotorOutputs)
	{
		Output = 0.f;
	}
	for (FForgeMultirotorMotor& Motor : Motors)
	{
		Motor.Output = 0.f;
	}
}

void AForgeMultirotorVehicle::SetMotorEnabled(const int32 MotorIndex, const bool bEnabled)
{
	if (!HasAuthority() || !Motors.IsValidIndex(MotorIndex) || Motors[MotorIndex].bEnabled == bEnabled)
	{
		return;
	}

	Motors[MotorIndex].bEnabled = bEnabled;

	if (bEnabled)
	{
		MotorEnabledMask |= (1 << MotorIndex);
	}
	else
	{
		MotorEnabledMask &= ~(1 << MotorIndex);
		Motors[MotorIndex].Output = 0.f;
		if (MotorOutputs.IsValidIndex(MotorIndex))
		{
			MotorOutputs[MotorIndex] = 0.f;
		}
		OnMotorFailed.Broadcast(MotorIndex);
	}
}

void AForgeMultirotorVehicle::OnRep_MotorEnabledMask()
{
	// Mirror the authority's motor states so client-side propeller visuals stop with the motor.
	for (int32 Index = 0; Index < Motors.Num(); ++Index)
	{
		const bool bEnabled = (MotorEnabledMask & (1 << Index)) != 0;
		if (Motors[Index].bEnabled != bEnabled)
		{
			Motors[Index].bEnabled = bEnabled;
			if (!bEnabled)
			{
				Motors[Index].Output = 0.f;
				OnMotorFailed.Broadcast(Index);
			}
		}
	}
}

void AForgeMultirotorVehicle::SetPowerScale(const float NewPowerScale)
{
	PowerScale = FMath::Clamp(NewPowerScale, 0.f, 1.f);
}

// ---------------------------------------------------------------- queries

float AForgeMultirotorVehicle::GetHoverThrottle() const
{
	TArray<ForgeDrone::Multirotor::FMotorGeometry> Geometry;
	BuildMotorGeometry(Geometry);
	const float Available = ForgeDrone::Multirotor::TotalAvailableThrustN(Geometry) * PowerScale;
	return ForgeDrone::Multirotor::HoverThrottle(MassKg, Available);
}

float AForgeMultirotorVehicle::GetThrustToWeightRatio() const
{
	TArray<ForgeDrone::Multirotor::FMotorGeometry> Geometry;
	BuildMotorGeometry(Geometry);
	return ForgeDrone::Multirotor::ThrustToWeightRatio(MassKg, Geometry) * PowerScale;
}

float AForgeMultirotorVehicle::GetMeanMotorOutput() const
{
	if (MotorOutputs.Num() == 0)
	{
		return 0.f;
	}
	float Total = 0.f;
	for (const float Output : MotorOutputs)
	{
		Total += Output;
	}
	return Total / MotorOutputs.Num();
}

// ---------------------------------------------------------------- simulation

void AForgeMultirotorVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const bool bSimulateLocally =
		ReplicationMethod == EForgeMultirotorReplicationType::ERT_None || HasAuthority();

	if (bSimulateLocally)
	{
		UpdatePhysics(DeltaTime);
		if (ReplicationMethod != EForgeMultirotorReplicationType::ERT_None && HasAuthority())
		{
			ServerStateSync();
		}
	}
	else if (ReplicationMethod == EForgeMultirotorReplicationType::ERT_Full)
	{
		ClientStateSync();
	}

	// Propeller visuals run everywhere - they are cosmetic and driven by replicated outputs.
	UpdateRotorVisuals();
}

void AForgeMultirotorVehicle::UpdatePhysics(const float DeltaTime)
{
	if (!Core || DeltaTime <= 0.f)
	{
		return;
	}

	// Wind and drag act on the airframe whether or not the motors are running, which is what makes
	// a dead drone tumble and drift rather than drop straight down.
	ApplyAerodynamicDrag(DeltaTime);

	if (!bMotorsArmed || PowerScale <= 0.f)
	{
		for (int32 Index = 0; Index < MotorOutputs.Num(); ++Index)
		{
			MotorOutputs[Index] = 0.f;
			if (Motors.IsValidIndex(Index))
			{
				Motors[Index].Output = 0.f;
			}
		}
		return;
	}

	float PitchCommand = 0.f;
	float RollCommand = 0.f;
	float YawCommand = 0.f;
	ComputeAttitudeCommands(DeltaTime, PitchCommand, RollCommand, YawCommand);

	const float Collective = ComputeCollective(DeltaTime);

	TArray<ForgeDrone::Multirotor::FMotorGeometry> Geometry;
	BuildMotorGeometry(Geometry);

	ForgeDrone::Multirotor::FMixAuthority Authority;
	Authority.Pitch = PitchAuthority;
	Authority.Roll = RollAuthority;
	Authority.Yaw = YawAuthority;

	ForgeDrone::Multirotor::MixMotors(Collective, PitchCommand, RollCommand, YawCommand, Geometry, Authority, MotorOutputs);

	for (int32 Index = 0; Index < Motors.Num() && Index < MotorOutputs.Num(); ++Index)
	{
		Motors[Index].Output = MotorOutputs[Index];
	}

	ApplyMotorForces();
}

void AForgeMultirotorVehicle::ComputeAttitudeCommands(const float DeltaTime, float& OutPitchCommand, float& OutRollCommand, float& OutYawCommand)
{
	// Body angular velocity in degrees/second: X roll, Y pitch, Z yaw.
	const FVector AngularVelocityDeg = Core->GetPhysicsAngularVelocityInDegrees();
	const FVector LocalAngularVelocity = GetActorTransform().InverseTransformVectorNoScale(AngularVelocityDeg);

	float TargetPitchRate = 0.f;
	float TargetRollRate = 0.f;
	const float TargetYawRate = YawStick * AcroMaxRatesDps.Z;

	if (FlightMode == EForgeMultirotorFlightMode::Acro)
	{
		// Rate mode: the stick *is* the rotation rate, with no levelling. This is what lets an FPV
		// quad flip and roll continuously.
		TargetPitchRate = PitchStick * AcroMaxRatesDps.X;
		TargetRollRate = RollStick * AcroMaxRatesDps.Y;
	}
	else
	{
		// Angle mode: outer proportional loop converts attitude error into a rate target, so
		// releasing the stick returns the aircraft to level.
		const FRotator CurrentRotation = GetActorRotation();
		const float TargetPitchDeg = PitchStick * MaxTiltAngleDeg;
		const float TargetRollDeg = RollStick * MaxTiltAngleDeg;

		const float PitchErrorDeg = FRotator::NormalizeAxis(TargetPitchDeg - CurrentRotation.Pitch);
		const float RollErrorDeg = FRotator::NormalizeAxis(TargetRollDeg - CurrentRotation.Roll);

		TargetPitchRate = FMath::Clamp(PitchErrorDeg * AngleModeLevelGain, -AcroMaxRatesDps.X, AcroMaxRatesDps.X);
		TargetRollRate = FMath::Clamp(RollErrorDeg * AngleModeLevelGain, -AcroMaxRatesDps.Y, AcroMaxRatesDps.Y);
	}

	// Inner rate loops close on measured body rates for both modes.
	OutPitchCommand = PitchRatePID.Update(TargetPitchRate - LocalAngularVelocity.Y, DeltaTime);
	OutRollCommand = RollRatePID.Update(TargetRollRate - LocalAngularVelocity.X, DeltaTime);
	OutYawCommand = YawRatePID.Update(TargetYawRate - LocalAngularVelocity.Z, DeltaTime);
}

float AForgeMultirotorVehicle::ComputeCollective(const float DeltaTime)
{
	if (FlightMode == EForgeMultirotorFlightMode::Acro)
	{
		// Direct throttle: the pilot owns the collective entirely, as on a real FPV rig. The stick
		// arrives as [-1, 1] from a centred axis, so remap it onto the [0, 1] throttle range.
		return FMath::Clamp((ThrottleStick + 1.f) * 0.5f, 0.f, 1.f);
	}

	// Angle mode: the stick commands a climb rate around the hover feed-forward, so releasing it
	// holds altitude instead of sinking.
	const float TargetClimbMS = ThrottleStick * ClimbRateMS;
	const float CurrentClimbMS = Core->GetPhysicsLinearVelocity().Z / 100.f;
	const float Correction = ClimbRatePID.Update(TargetClimbMS - CurrentClimbMS, DeltaTime);

	return FMath::Clamp(GetHoverThrottle() + Correction, 0.f, 1.f);
}

void AForgeMultirotorVehicle::ApplyMotorForces()
{
	const FTransform ActorTransform = GetActorTransform();
	const FVector BodyUp = ActorTransform.GetUnitAxis(EAxis::Z);

	for (int32 Index = 0; Index < Motors.Num() && Index < MotorOutputs.Num(); ++Index)
	{
		const FForgeMultirotorMotor& Motor = Motors[Index];
		if (!Motor.bEnabled || MotorOutputs[Index] <= 0.f)
		{
			continue;
		}

		const float ThrustN = MotorOutputs[Index] * Motor.MaxThrustN * PowerScale;
		const FVector MotorWorldLocation = ActorTransform.TransformPosition(Motor.RelativeLocationCm);

		// Applied at the motor's own location, so the offset between motors produces the pitch and
		// roll moments naturally, and leaning the airframe redirects the whole thrust vector.
		Core->AddForceAtLocation(BodyUp * ThrustN * NewtonsToUnreal, MotorWorldLocation);
	}
}

void AForgeMultirotorVehicle::ApplyAerodynamicDrag(const float DeltaTime)
{
	const FVector VelocityCmS = Core->GetPhysicsLinearVelocity();

	// Drag and wind are the same force: both act on the air's velocity relative to the airframe.
	// Handling them together is why a hovering drone drifts downwind while apparently holding still,
	// and why it has to lean into a breeze to stay put.
	if (const UForgeWindSubsystem* Wind = UForgeWindSubsystem::Get(this))
	{
		const FVector Force = Wind->ComputeWindForce(GetActorLocation(), VelocityCmS, BodyDragCoefficient, FrontalAreaM2, AirDensity);
		if (!Force.IsNearlyZero())
		{
			Core->AddForce(Force);
		}
		return;
	}

	// No wind subsystem (bare plugin use): fall back to plain drag opposing motion.
	const FVector VelocityMS = VelocityCmS / 100.f;
	if (VelocityMS.IsNearlyZero())
	{
		return;
	}
	const float Speed = VelocityMS.Size();
	const float DragMagnitudeN = 0.5f * AirDensity * BodyDragCoefficient * FrontalAreaM2 * Speed * Speed;
	Core->AddForce(-VelocityMS.GetSafeNormal() * DragMagnitudeN * NewtonsToUnreal);
}

void AForgeMultirotorVehicle::UpdateRotorVisuals()
{
	// Simulating machines have real per-motor outputs. Observers do not - replicating four floats
	// per drone per frame for cosmetics would be wasteful, so they spin the props from the
	// replicated collective instead. Close enough to read as "this drone is working hard", and a
	// motor knocked out still visibly stops because the enable mask *is* replicated.
	const bool bHasLocalOutputs =
		ReplicationMethod == EForgeMultirotorReplicationType::ERT_None || HasAuthority() || IsLocallyControlled();

	const float ObservedCollective = bMotorsArmed
		? FMath::Clamp((ServerState.ServerThrottle + 1.f) * 0.5f, 0.f, 1.f)
		: 0.f;

	for (int32 Index = 0; Index < CachedRotors.Num() && Index < Motors.Num(); ++Index)
	{
		URotorComponent* Rotor = CachedRotors[Index];
		if (!Rotor)
		{
			continue;
		}

		const float Output = Motors[Index].bEnabled
			? (bHasLocalOutputs ? Motors[Index].Output : ObservedCollective)
			: 0.f;

		// Reuse the rotary-wing rotor purely as a spinning mesh; its own physics path is not used.
		Rotor->UpdateAnimation(Output * 100.f, 1.f);
	}
}

// ---------------------------------------------------------------- replication

void AForgeMultirotorVehicle::ServerStateSync()
{
	if (!Core)
	{
		return;
	}
	ServerState.ServerLinearVelocity = Core->GetPhysicsLinearVelocity();
	ServerState.ServerAngularVelocity = Core->GetPhysicsAngularVelocityInDegrees();
	ServerState.ServerTransform = GetActorTransform();
	ServerState.ServerThrottle = ThrottleStick;
	ServerState.ServerPitch = PitchStick;
	ServerState.ServerRoll = RollStick;
	ServerState.ServerYaw = YawStick;
}

void AForgeMultirotorVehicle::ClientStateSync()
{
	if (!Core || IsLocallyControlled())
	{
		// The controlling client keeps simulating its own aircraft; correcting it every frame from
		// a stale snapshot would feel worse than the small divergence it avoids.
		return;
	}

	SetActorTransform(ServerState.ServerTransform, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
	Core->SetPhysicsLinearVelocity(ServerState.ServerLinearVelocity);
	Core->SetPhysicsAngularVelocityInDegrees(ServerState.ServerAngularVelocity);

	// Keep the replicated sticks so propeller visuals and audio track what the pilot is doing.
	ThrottleStick = ServerState.ServerThrottle;
	PitchStick = ServerState.ServerPitch;
	RollStick = ServerState.ServerRoll;
	YawStick = ServerState.ServerYaw;
}
