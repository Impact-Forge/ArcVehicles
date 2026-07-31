//Copyright 2024 H.Kallisto
//1.0.

#include "ForgeRotaryWingVehicle.h"
#include "Kismet/KismetMathLibrary.h"
#include "RotorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

// Sets default values
AForgeRotaryWingVehicle::AForgeRotaryWingVehicle()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	RootComponent = Core;

	TransformedAxis = CreateDefaultSubobject<USceneComponent>(TEXT("Transform"));
	TransformedAxis->SetupAttachment(RootComponent);

	// This class synchronises its own state through FForgeRotaryWingServerState, so engine movement
	// replication (inherited on from AForgeVehicle) would be a second, conflicting source of truth.
	// Matches how the fixed-wing vehicle sets itself up.
	SetReplicateMovement(false);
}

// Called when the game starts or when spawned
void AForgeRotaryWingVehicle::BeginPlay()
{
	Super::BeginPlay();
	Mass = Core->GetMass();

	Core->SetAngularDamping(RotationalDamping);
	Core->SetLinearDamping(LinearDamping);
	Core->SetSimulatePhysics(true);

	TArray<UActorComponent*> comps;
	GetComponents(URotorComponent::StaticClass(), comps);

	if (comps.Num() == 0) { return; }

	for (int i = 0; i < comps.Num(); i++)
	{
		URotorComponent* rotorComp = Cast<URotorComponent>(comps[i]);
		Rotors.Add(rotorComp);
	}

	bTakingOff = bInitalTakeoff;
}

void AForgeRotaryWingVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AForgeRotaryWingVehicle, ServerState);
}

void AForgeRotaryWingVehicle::UpdatePhysics()
{
	Core->WakeAllRigidBodies();

	FRotator a = GetActorRotation();
	FVector r = Core->GetRightVector();
	FVector f = Core->GetForwardVector();
	FVector u = Core->GetUpVector();

	FVector vVector = UKismetMathLibrary::InverseTransformDirection(GetTransform(), GetVelocity());
	Velocity = vVector.X / 100.f;

	FRotator transformRotation = FRotator(0.f, a.Yaw, 0.f);
	TransformedAxis->SetWorldRotation(transformRotation);

	float dynMass = Mass + AddedMass;

	if (bTakingOff)
	{
		bLanded = false;
		DynamicLiftForce = DynamicLiftForce + (TakeOffSpeed * GameThreadDeltaTime);
		TakeOffRatio = DynamicLiftForce / dynMass;
		FVector initLift = DynamicLiftForce * 980.f * u;
		Core->AddForce(initLift);

		if (DynamicLiftForce >= dynMass - (0.05f * dynMass))
		{
			bTakingOff = false;
			TakeOffRatio = 1.f;
			bLanded = false;
			DynamicLiftForce = 0.f;
		}
		//bTakingOff = (DynamicLiftForce >= (dynMass - (0.05f * dynMass)));
	}

	if (bLanding && !bLanded)
	{
		DynamicLandingForce = DynamicLandingForce + (LandingSpeed * GameThreadDeltaTime);
		TakeOffRatio = 1.f - (DynamicLandingForce / dynMass);
		FVector dynamicLift = FMath::Clamp((dynMass- DynamicLandingForce),0.f,dynMass) * 980.f * u;
		Core->AddForce(dynamicLift);

		if (DynamicLandingForce >= dynMass - (0.0005f * dynMass))
		{
			bLanded = true;
			bLanding = false;
			TakeOffRatio = 0.f;
			DynamicLandingForce = 0.f;
		}
	}

	if (!bLanding && !bTakingOff && !bLanded)
	{
		float m = bUseLift ? dynMass : 0.f;
		FVector lift = (m + (ElevationInput * ElevatorRate) + (-PitchInput * PitchDeclineRate)) * 980.f * u;
		Core->AddForce(lift);
	}

	if (!bLanded && !bLanding)
	{
		FVector2D pitchIn = FVector2D(0.f, PitchThreshold);
		FVector2D pitchOut = FVector2D(1.f, 0.f);
		float secondaryPitch = FMath::GetMappedRangeValueClamped(pitchIn, pitchOut, FMath::Abs(a.Pitch));
		float validPitchInput = (PitchInput < 0.f && a.Pitch < 0.f) || (PitchInput > 0.f && a.Pitch > 0.f) ? 1.f : secondaryPitch;
		if (!bUseTransformedAxisStability && !bLanding) { validPitchInput = 1.f; }
		float pitchTorque = PitchRate * PitchInput * validPitchInput;
		Core->AddTorqueInDegrees(r * pitchTorque, NAME_None, true);

		FVector2D rollIn = FVector2D(0.f, RollThreshold);
		FVector2D rollOut = FVector2D(1.f, 0.f);
		float secondaryRoll = FMath::GetMappedRangeValueClamped(rollIn, rollOut, FMath::Abs(a.Roll));
		float validRollInput = (RollInput < 0.f && a.Roll > 0.f) || (RollInput > 0.f && a.Roll < 0.f) ? 1.f : secondaryRoll;
		if (!bUseTransformedAxisStability && !bLanding) { validRollInput = 1.f; }
		float rollTorque = RollRate * RollInput * validRollInput;
		Core->AddTorqueInDegrees(-1.f * f * rollTorque, NAME_None, true);

		Core->AddTorqueInDegrees(YawInput * YawRate * -1.f * u, NAME_None, true);


		float longForce = LongitudinalForce * PitchInput * 100.f;
		Core->AddForce(TransformedAxis->GetForwardVector() * longForce, NAME_None, false);

		float latForce = LateralForce * RollInput * 100.f;
		Core->AddForce(TransformedAxis->GetRightVector() * latForce, NAME_None, false);
	}

	if (bUseTransformedAxisStability)
	{
		float pitchCorrection = FMath::Lerp(StabilizationSpeed, 0.f, FMath::Abs(PitchInput));
		Core->AddTorqueInDegrees(r * a.Pitch * pitchCorrection, NAME_None, true);

		float rollCorrection = FMath::Lerp(StabilizationSpeed, 0.f, FMath::Abs(RollInput));
		Core->AddTorqueInDegrees(f * a.Roll * rollCorrection, NAME_None, true);
	}


	Drag = 0.5f * AirDensity * DragArea * FMath::Pow(FMath::Abs(GetVelocityMS()), 2) * DragCoefficent * 100.f;
	FVector dragForce = f * -FMath::Sign(Velocity) * Drag;

	Core->AddForce(dragForce, NAME_None, false);

	if (bUseLift && !bLanding && !bTakingOff && !bLanded)
	{
		FVector velocity = UKismetMathLibrary::InverseTransformDirection(GetActorTransform(), GetVelocity());
		float slip = velocity.Y / velocity.X;
		float direction = -FMath::Sign(slip);
		Core->AddForce(r * direction * SlipCorrection);
	}

}

void AForgeRotaryWingVehicle::UpdateDynamicStability(TArray<float> inputs)
{
	StabilityAxisCount = 0;

	for (int i = 0; i < inputs.Num(); i++)
	{
		if (FMath::Abs(inputs[i]) > 0)
		{
			StabilityAxisCount++;
		}
	}

	bUseTransformedAxisStability = (StabilityAxisCount == 0);
}

// Called every frame
void AForgeRotaryWingVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	GameThreadDeltaTime = DeltaTime;

	if (ReplicationMethod == EForgeRotaryWingReplicationType::ERT_None)
	{
		UpdatePhysics();
	}
	else if (ReplicationMethod == EForgeRotaryWingReplicationType::ERT_K2)
	{
		if (HasAuthority())
		{
			UpdatePhysics();
			ServerStateSync(true);
		}
		else
		{
			ClientStateSync(true);
		}
	}	
	else if (ReplicationMethod == EForgeRotaryWingReplicationType::ERT_DataOnly)
	{
		if (HasAuthority())
		{
			UpdatePhysics();
			ServerStateSync(false);
		}
		else
		{
			ClientStateSync(false);
		}
	}

#pragma region Rotors

	// Rotor thrust must follow the same authority rule as UpdatePhysics above. When the vehicle is
	// server-synced (ERT_K2 / ERT_DataOnly), a simulating client applying its own rotor forces fights
	// the incoming state every frame. Animation still runs everywhere - it is purely visual.
	const bool bApplyRotorPhysics =
		ReplicationMethod == EForgeRotaryWingReplicationType::ERT_None || HasAuthority();

	for (int i = 0; i < Rotors.Num(); i++)
	{
		Rotors[i]->UpdateAnimation(Velocity, TakeOffRatio);

		if (bApplyRotorPhysics)
		{
			Rotors[i]->UpdatePhysics(GameThreadDeltaTime, Core, TakeOffRatio);
		}
	}

#pragma endregion
}

void  AForgeRotaryWingVehicle::ServerStateSync(bool updatePhysics)
{
	if (updatePhysics)
	{
		ServerState.ServerLinearVelocity = Core->GetPhysicsLinearVelocity();
		ServerState.ServerAngularVelocity = Core->GetPhysicsAngularVelocityInDegrees();
		ServerState.ServerTransform = GetTransform();
	}

	ServerState.ServerPitchInput = PitchInput;
	ServerState.ServerYawInput = YawInput;
	ServerState.ServerRollInput = RollInput;
	ServerState.ServerElevatorInput = ElevationInput;
}

void  AForgeRotaryWingVehicle::ClientStateSync(bool updatePhysics)
{
	if (updatePhysics)
	{
		Core->SetPhysicsLinearVelocity(ServerState.ServerLinearVelocity);
		Core->SetPhysicsAngularVelocityInDegrees(ServerState.ServerAngularVelocity);
	}

	PitchInput = ServerState.ServerPitchInput;
	YawInput = ServerState.ServerYawInput;
	RollInput = ServerState.ServerRollInput;
	ElevationInput = ServerState.ServerElevatorInput;
}

float AForgeRotaryWingVehicle::GetControllable()
{
	return bInputEnabled ? 1.f : 0.f;
}

void AForgeRotaryWingVehicle::SetAddedMassAmount(float massAmount)
{
	AddedMass = massAmount;
}

void AForgeRotaryWingVehicle::SetIsLanding()
{
	bLanding = true;
	bLanded = false;
	ServerSetIsLanding();
}

void AForgeRotaryWingVehicle::ServerSetIsLanding_Implementation()
{
	bLanding = true;
	bLanded = false;
}


bool AForgeRotaryWingVehicle::ServerSetIsLanding_Validate()
{
	return true;
}


void AForgeRotaryWingVehicle::SetIsTakingOff()
{
	bTakingOff = true;
	ServerSetIsTakingOff();
}


void AForgeRotaryWingVehicle::ServerSetIsTakingOff_Implementation()
{
	bTakingOff = true;
}


bool AForgeRotaryWingVehicle::ServerSetIsTakingOff_Validate()
{
	return true;
}


void AForgeRotaryWingVehicle::SetPitchInput(float value)
{
	PitchInput = value * GetControllable();
	ServerSetPitchInput(PitchInput);
}

void AForgeRotaryWingVehicle::ServerSetPitchInput_Implementation(float value)
{
	PitchInput = value * GetControllable();
}

bool AForgeRotaryWingVehicle::ServerSetPitchInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

void AForgeRotaryWingVehicle::SetYawInput(float value)
{
	YawInput = value * GetControllable();
	ServerSetYawInput(YawInput);
}

void AForgeRotaryWingVehicle::ServerSetYawInput_Implementation(float value)
{
	YawInput = value * GetControllable();
}

bool AForgeRotaryWingVehicle::ServerSetYawInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

void AForgeRotaryWingVehicle::SetRollInput(float value)
{
	RollInput = value * GetControllable();
	ServerSetRollInput(RollInput);
}

void AForgeRotaryWingVehicle::ServerSetRollInput_Implementation(float value)
{
	RollInput = value * GetControllable();
}

bool AForgeRotaryWingVehicle::ServerSetRollInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}


void AForgeRotaryWingVehicle::SetElevatorInput(float value)
{
	ElevationInput = value * GetControllable();
	ServerSetElevatorInput(ElevationInput);
}

void AForgeRotaryWingVehicle::ServerSetElevatorInput_Implementation(float value)
{
	ElevationInput = value * GetControllable();
}

bool AForgeRotaryWingVehicle::ServerSetElevatorInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

// Called to bind functionality to input
void AForgeRotaryWingVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

float  AForgeRotaryWingVehicle::SmoothThrust(float rotorThrust, float newThrust, float speed)
{
	return FMath::FInterpTo(rotorThrust, newThrust, GameThreadDeltaTime, speed);
}

void AForgeRotaryWingVehicle::SetControllable(bool inputEnabled)
{
	bInputEnabled = inputEnabled;
	ServerSetControllable(bInputEnabled);
}

void AForgeRotaryWingVehicle::ServerSetControllable_Implementation(bool inputEnabled)
{
	bInputEnabled = inputEnabled;
}


bool AForgeRotaryWingVehicle::ServerSetControllable_Validate(bool inputEnabled)
{
	return true;
}


float AForgeRotaryWingVehicle::GetVelocityMS()
{
	return Velocity;
}


float AForgeRotaryWingVehicle::GetSpeedKPH()
{
	return Velocity * 3.6f;
}


float AForgeRotaryWingVehicle::GetSpeedKnots()
{
	return Velocity * 1.94384f;
}

float AForgeRotaryWingVehicle::GetDrag()
{
	return (Drag / 100.f) / 9.8f;
}

bool AForgeRotaryWingVehicle::IsTakingOff()
{
	return bTakingOff;
}

bool AForgeRotaryWingVehicle::IsLanding()
{
	return bLanding;
}

bool AForgeRotaryWingVehicle::GetIsLanded()
{
	return bLanded;
}