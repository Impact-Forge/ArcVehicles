//Copyright 2024 P.Kallisto

#include "Vehicle/ForgeGroundVehicle.h"
#include "Vehicle/SuspensionComponent.h"
#include "Vehicle/ForgeGroundInteriorComponent.h"
#include "Vehicle/ArticulatedComponent.h"
#include "Vehicle/StaticVehicleWheel.h"
#include "Vehicle/ForgeGroundVehicleWheel.h"
#include "AsyncTickFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveFloat.h"
#include "Net/UnrealNetwork.h"
#include "Runtime/Launch/Resources/Version.h"


AForgeGroundVehicle::AForgeGroundVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

#if (ENGINE_MAJOR_VERSION > 5) || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
	// UE-5.5+ API
	SetNetUpdateFrequency(100);
	SetMinNetUpdateFrequency(100);
#else
	// UE-5.2 through 5.4 API
	NetUpdateFrequency = 100;
	MinNetUpdateFrequency = 100;

#endif

}


void AForgeGroundVehicle::BeginPlay()
{
	Super::BeginPlay();

	PrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent());

	if (PrimitiveComponent == nullptr) { UE_LOG(LogTemp, Error, TEXT("No primitive component as the root object of this vehicle found!")); }

	BrakeForceDuplicate = BrakeForce;
	EngineTorqueDuplicate = EngineTorque;

	//TArray<UActorComponent*> comps = this->GetComponentsByClass(USuspensionComponent::StaticClass());
	TArray<UActorComponent*> comps;
	GetComponents(USuspensionComponent::StaticClass(), comps);

	if (comps.Num() != 0)
	{
		for (int i = 0; i < comps.Num(); i++)
		{
			USuspensionComponent* suspComp = Cast<USuspensionComponent>(comps[i]);
			SuspensionComponents.Add(suspComp);

			if (ReplicationMethod == EReplicationType::ERT_Full || ReplicationMethod == EReplicationType::ERT_DataOnly)
			{
				suspComp->SetIsReplicated(true);
			}
		}
	}

	TArray<UActorComponent*> interiorComps;
	GetComponents(UForgeGroundInteriorComponent::StaticClass(), interiorComps);
	if (interiorComps.Num() != 0)
	{
		for (int j = 0; j < interiorComps.Num(); j++)
		{
			UForgeGroundInteriorComponent* icomp = Cast<UForgeGroundInteriorComponent>(interiorComps[j]);
			InteriorComponents.Add(icomp);
		}
	}

	TArray<UActorComponent*> artComps;
	GetComponents(UArticulatedComponent::StaticClass(), artComps);
	if (artComps.Num() != 0)
	{
		for (int z = 0; z < artComps.Num(); z++)
		{
			UArticulatedComponent* art = Cast<UArticulatedComponent>(artComps[z]);
			ArticulatedComponents.Add(art);
		}
	}

	World = GetWorld();
}


void AForgeGroundVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	GameDeltaTime = DeltaTime;

	if (PrimitiveComponent == nullptr) { return; }

	if (bAutoWakeRigidBodies)
	{
		PrimitiveComponent->WakeAllRigidBodies();
	}
	
	AnimateSteering();

	if (SuspensionComponents.Num() > 0)
	{
		for (int i = 0; i < SuspensionComponents.Num(); i++)
		{
			SuspensionComponents[i]->UpdateTick(DeltaTime);
			SuspensionComponents[i]->SetHandbrakeInput(bHandbrakeInput);
		}
	}


	if (InteriorComponents.Num() > 0)
	{
		for (int j = 0; j < InteriorComponents.Num(); j++)
		{
			InteriorComponents[j]->Update(DeltaTime, RPM, GetSpeedKPH(), SteeringInput);
		}
	}

	if (ArticulatedComponents.Num() > 0)
	{
		for (int z = 0; z < ArticulatedComponents.Num(); z++)
		{
			ArticulatedComponents[z]->Update(DeltaTime);
		}
	}

	if (StaticWheels.Num() > 0)
	{
		for (int i = 0; i < StaticWheels.Num(); i++)
		{
			StaticWheels[i]->Update(DeltaTime);
		}

	}


	if (!HasAuthority() && bUseServerFastSync && ReplicationMethod == EReplicationType::ERT_Full)
	{
		FastServerSyncCount++;
		if (FastServerSyncCount > ServerFastSyncLatency)
		{
			FastServerSyncCount = 0;
			SetActorTransform(ServerState.ServerTransform);
		}
	}

	if (AntiGravity != nullptr)
	{
		AntiGravityMass = AntiGravityCoreMesh->GetMass();
		FRotator r = PrimitiveComponent->GetComponentRotation();
		AntiGravity->SetWorldRotation(FRotator
		(
			0.f,
			r.Yaw,
			0.f
			
		));
	}

	LocalVelocityGT = UKismetMathLibrary::InverseTransformDirection(GetActorTransform(), GetVelocity());

	FTransform t = GetActorTransform();
	PreviousLocation = CurrentLocation;
	CurrentLocation = t.GetLocation();
	float deltaTime = 1 / 60.f;

	FVector worldlocationVelocity = (CurrentLocation - PreviousLocation) * (1.f / 100.f) * (1.f / AnimDeltaTime);
	LocationVelocity = UKismetMathLibrary::InverseTransformDirection(t, worldlocationVelocity);

	if (bEnableAnimationMode)
	{
		if (SuspensionComponents.Num() == 0) { return; }

		for (int i = 0; i < SuspensionComponents.Num(); i++)
		{
			float angularVelocity = (LocationVelocity.X) / (SuspensionComponents[i]->GetWheelRadius() * -1.f);
			float d = SuspensionComponents[i]->IsLeftWheel() ? -1.f : 1.f;


			//Multiply by 100 to convert to meters from cm.
			SuspensionComponents[i]->GetWheelMeshComponent()->AddLocalRotation(
				FRotator(
					(angularVelocity + SuspensionComponents[i]->GetBurnoutRotation()) * 100.f * d * SuspensionComponents[i]->GetAngularVelocityMultiplier(),
					0.f,
					0.f
				));
		}
	}

#pragma region DrawDebug
	/*
	FVector com = UAsyncTickFunctions::ATP_GetCOM(PrimitiveComponent);
	FVector up = PhysicsTransform.GetRotation().GetUpVector();
	DrawDebugLine(GetWorld(), com, com + up * 100.f, FColor::Blue, false, -1.0f, 0.0f,3.f);
	*/
#pragma endregion
}


void AForgeGroundVehicle::InitializeAnimationSystem()
{
	TArray<USceneComponent*> comps;
	RootComponent->GetChildrenComponents(true, comps);

	SuspensionComponents.Empty();

	if (!comps.IsEmpty())
	{
		for (int i = 0; i < comps.Num(); i++)
		{
			if (comps[i]->ComponentTags.Num() > 0)
			{
				if (comps[i]->ComponentTags[0] == FName("SuspensionComponent"))
				{
					USuspensionComponent* suspComp = Cast<USuspensionComponent>(comps[i]);

					SuspensionComponents.Add(suspComp);
				} 
			}
		}
	}

	TArray<USceneComponent*> wcomps;
	RootComponent->GetChildrenComponents(true, wcomps);
	AnimWheels.Empty();

	if (!wcomps.IsEmpty())
	{
		for (int i = 0; i < wcomps.Num(); i++)
		{
			if (wcomps[i]->ComponentTags.Num() > 0)
			{
				if (wcomps[i]->ComponentTags[0] == FName("ForgeVehiclesGroundWheel"))
				{
					UForgeGroundVehicleWheel* animWheel = Cast<UForgeGroundVehicleWheel>(wcomps[i]);

					AnimWheels.Add(animWheel);
				}
			}
		}
	}

}


void AForgeGroundVehicle::SimulateInternalAnimation()
{
	FTransform t = GetActorTransform();
	PreviousLocation = CurrentLocation;
	CurrentLocation = t.GetLocation();
	float deltaTime = 1 / 60.f;

	FVector worldlocationVelocity = (CurrentLocation - PreviousLocation) * (1.f / 100.f) * (1.f / AnimDeltaTime);
	LocationVelocity = UKismetMathLibrary::InverseTransformDirection(t, worldlocationVelocity);

	if (bEnableAnimationMode)
	{
		SimulateInternalWheelAnimation();
	}

}


void AForgeGroundVehicle::SimulateInternalWheelAnimation()
{
	if (AnimWheels.Num() == 0) { return; }

	for (int i = 0; i < AnimWheels.Num(); i++)
	{
		float radius = (AnimWheels[i]->GetStaticMesh()->GetBoundingBox().GetSize().Z) / 2.f;
		float angularVelocity = (LocationVelocity.X) / (radius * -1.f);
		float d = AnimWheels[i]->IsLeftWheel() ? -1.f : 1.f;

		AnimWheels[i]->AddLocalRotation(
			FRotator(
				angularVelocity * 100.f * d * AnimationGlobalWheelSpeedMultiplier,
				0.f,
				0.f
			));			
	}	

}

void AForgeGroundVehicle::InitializeAntiGravity(USceneComponent* antiGravityComponent, UStaticMeshComponent* coreMesh)
{
	if (antiGravityComponent == nullptr)
	{

		UE_LOG(LogTemp, Error, TEXT("No scene component in InitializeAntiGravity function! Please set a scene component to use the Anti Gravity system"));
		return; 
	}
	if (coreMesh == nullptr)
	{ 
		UE_LOG(LogTemp, Error, TEXT("No core mesh component in InitializeAntiGravity function! Please set a core to use the Anti Gravity system"));
		return;
	}

	AntiGravity = antiGravityComponent;
	AntiGravityCoreMesh = coreMesh;
}

void AForgeGroundVehicle::InitializeLeanSteer(USuspensionComponent* suspensionComponent)
{
	if (suspensionComponent == nullptr)
	{

		UE_LOG(LogTemp, Error, TEXT("No suspension component in InitializeLeanSteer function! Please set a suspension component to use the Lean Steer system"));
		return;

	}
	else
	{
		LeanSteerSuspension = suspensionComponent;
	}

}


void AForgeGroundVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AForgeGroundVehicle, ServerState);
	DOREPLIFETIME(AForgeGroundVehicle, ServerVelocity);
	DOREPLIFETIME(AForgeGroundVehicle, ServerAngularVelocity);

}

void AForgeGroundVehicle::NativeAsyncTick(float DeltaTime)
{
	Super::NativeAsyncTick(DeltaTime);
	if (PrimitiveComponent == nullptr) { return; }

	AsyncDeltaTime = DeltaTime;
	//if (GetLocalRole() == ROLE_SimulatedProxy) { return; }

	float AsyncTime = DeltaTime;

	int32 c = 0;

	if (bUseDrivetrain)
	{
		DrivetrainWheelCount = 0.f;
		float totalSlip = 0.f;
		float totalSlipGamma = 0.f;

		for (int i = 0; i < SuspensionComponents.Num(); i++)
		{
			if (SuspensionComponents[i]->GetIsDrivenWheel())
			{
				DrivetrainWheelCount = DrivetrainWheelCount + 1.f;
				totalSlip = totalSlip + SuspensionComponents[i]->GetSlipRatio();
				totalSlipGamma = totalSlipGamma + SuspensionComponents[i]->GetSlipGamma();
			}

			if (!SuspensionComponents[i]->IsInAir() && SuspensionComponents[i]->GetIsDrivenWheel())
			{
				c++;
			}
		}


		AveragedSlip = totalSlip / DrivetrainWheelCount;

		AveragedSlipGamma = totalSlipGamma / DrivetrainWheelCount;

		bInAir = !(c >= 1);


	}
	else	{

		for (int i = 0; i < SuspensionComponents.Num(); i++)
		{
			if (SuspensionComponents[i]->IsInAir())
			{
				c++;
			}
		}

		bInAir = c >= InAirWheelCount;
	}


	LastAngularVelocity = AngularVelocity;
	AngularVelocity = UKismetMathLibrary::InverseTransformDirection(PhysicsTransform, UAsyncTickFunctions::ATP_GetAngularVelocity(PrimitiveComponent)).Z;
	AngularAcceleration = (AngularVelocity - LastAngularVelocity) * (1.f / AsyncDeltaTime);


	if (FMath::Abs(GetSpeedKPH()) < IdleLockSpeedThreshold && Throttle == 0 && Brakes == 0 && !bInAir && FMath::Abs(AngularVelocity) < 0.15f)
	{
		if (!bIsIdleLocked && bUseIdleLock)
		{
			IdleLockCount += AsyncTime;
		}

		if ((IdleLockCount > IdleLockTime) && bUseIdleLock)
		{
			bIsIdleLocked = true;
			IdleLockCount = 612.f;

			FVector v = UAsyncTickFunctions::ATP_GetLinearVelocity(PrimitiveComponent);
			float interpspeed = 5.f;
			float x = FMath::FInterpTo(v.X, 0.f, AsyncTime, interpspeed);
			float y = FMath::FInterpTo(v.Y, 0.f, AsyncTime, interpspeed);
			float z = FMath::FInterpTo(v.Z, 0.f, AsyncTime, interpspeed);

			if (ReplicationMethod == EReplicationType::ERT_None)
			{
				UAsyncTickFunctions::ATP_SetLinearVelocity(PrimitiveComponent, FVector(x, y, z), false);
				UAsyncTickFunctions::ATP_SetAngularVelocityInRadians(PrimitiveComponent, FVector::ZeroVector, false);
			}


			IdleLockRotationLocationCount += AsyncTime;
			if (IdleLockRotationLocationCount >= (IdleLockTime / 2.f))
			{
				bIsIdleLockedRotationLocation = true;

				FVector direction = v;
				direction.Normalize();
				UAsyncTickFunctions::ATP_AddForce(PrimitiveComponent, -direction * v.Size() * IdleLockStiffness * 100.f, false);
			}

		}
	}
	else
	{
		IdleLockCount = 0;
		IdleLockRotationLocationCount = 0;
		bIsIdleLocked = false;
		bIsIdleLockedRotationLocation = false;
	}

	if (ReplicationMethod == EReplicationType::ERT_Full)
	{

		if (HasAuthority())
		{
			UpdatePhysicsCore(DeltaTime);
			AsyncTick(DeltaTime);
			ServerStateSync(true);

		}
		else
		{
			ClientStateSync(true);
		}
	}
	else if (ReplicationMethod == EReplicationType::ERT_DataOnly)
	{
		if (HasAuthority())
		{
			UpdatePhysicsCore(DeltaTime);
			AsyncTick(DeltaTime);
			ServerStateSync(false);

		}
		else
		{
			ClientStateSync(false);
			UpdatePhysicsCore(DeltaTime);
			AsyncTick(DeltaTime);
		}
	}
	else if(ReplicationMethod == EReplicationType::ERT_None)
	{
		UpdatePhysicsCore(DeltaTime);
		AsyncTick(DeltaTime);
	}

	float torque = CurrentTorque * GetCurrentGearRatio() * GetDifferentialRatio() * GetThrottleInput() * WheelRadius;

	for (int i = 0; i < SuspensionComponents.Num(); i++)
	{
		if (SuspensionComponents[i] != nullptr)
		{
			SuspensionComponents[i]->UpdatePhysics(DeltaTime, torque, SlipInitiateSpeed, SlipRestoreSpeed, Throttle, Brakes);
			SuspensionComponents[i]->UpdateAnimationState(bEnableAnimationMode, LocationVelocity.X * AnimationGlobalWheelSpeedMultiplier);
		}
	}

}


void AForgeGroundVehicle::AsyncReplicatedTick()
{
	if (PrimitiveComponent == nullptr) { return; }

	if (HasAuthority())
	{
		ServerVelocity = UAsyncTickFunctions::ATP_GetLinearVelocity(PrimitiveComponent);
		ServerAngularVelocity = UAsyncTickFunctions::ATP_GetAngularVelocity(PrimitiveComponent);

	}
	else
	{
		UAsyncTickFunctions::ATP_SetLinearVelocity(PrimitiveComponent, ServerVelocity, false);
		UAsyncTickFunctions::ATP_SetAngularVelocityInRadians(PrimitiveComponent, ServerAngularVelocity, false);
	}
}


void AForgeGroundVehicle::UpdatePhysicsCore(float DeltaTime)
{
	PhysicsTransform = UAsyncTickFunctions::ATP_GetTransform(PrimitiveComponent);

	float d = FMath::Sign(GetLinearVelocity());
	PhysicsTransform = UAsyncTickFunctions::ATP_GetTransform(PrimitiveComponent);
	FVector fwd = PhysicsTransform.GetRotation().GetForwardVector();
	FVector rgt = PhysicsTransform.GetRotation().GetRightVector();
	FVector up = PhysicsTransform.GetRotation().GetUpVector();

	if (PowertrainType == EBehaviourType::Standard)
	{
		float r = WheelRadius;
		float xd = DifferentialRatio;
		float xg = 3.5f;
		float velocityDirection = FMath::Sign(GetLinearVelocity());
		float rpmDelta = 50;

		if (true)
		{
			switch (TorqueImplementation)
			{
			case ETorqueImplementation::ETI_Constant:
				CurrentTorque = EngineTorque;
				break;

			case ETorqueImplementation::ETI_Curve:

				if (EngineTorqueCurve == nullptr)
				{
					UE_LOG(LogTemp, Error, TEXT("No engine torque curve set in advanced vehicle powertrain! "));
					return;
				}
				else
				{
					CurrentTorque = EngineTorque * EngineTorqueCurve->GetFloatValue(FMath::Abs(RPM));
				}
				break;
			}

			if (GearRatios.Num() == 0)
			{
				xg = 3.5f;

			}
			else
			{
				xg = GearRatios[CurrentGear];
			}
			Xg = xg;


			float CONST = 2.f * PI * (1.f / 60.f);
			float gmax = MaxRPM * CONST * WheelRadius * (1.f / xg) * (1.f / xd) * 3.6f;
			float cg = RPM * CONST * WheelRadius * (1.f / xg) * (1.f / xd) * 3.6f;
			//UE_LOG(LogTemp, Log, TEXT("Max speed at current gear: %f ----- Current speed %f"), gmax, currentSpeedInGear);

			bool upShiftCondition = !bUseSpeedBasedShifting ? RPM > MaxRPM * UpshiftRatio : false;
			bool downShiftCondition = !bUseSpeedBasedShifting ? (RPM < MaxRPM * DownshiftRatio) : false;

			if (bUseSpeedBasedShifting)
			{
				if (SpeedShiftingRatios.Num() > 0)
				{
					if (CurrentGear < SpeedShiftingRatios.Num())
					{
						upShiftCondition = GetSpeedKPH() > SpeedShiftingRatios[CurrentGear].Y;

						if (CurrentGear >= 0)
						{
							downShiftCondition = GetSpeedKPH() < SpeedShiftingRatios[CurrentGear].X;
						}
					}
				}
			}

			if (bAutomatic && !bLockShift)
			{
				if (bUseDrivetrain)
				{
					if (GearRatios.Num() != 1)
					{

						if (upShiftCondition && !bInAir)
						{
							Shift(true);
						}

						if (downShiftCondition && !bInAir)
						{
							Shift(false);
						}

					}
				}
				else
				{
					if (GearRatios.Num() != 1)
					{
						if (upShiftCondition)
						{
							Shift(true);
						}

						if (downShiftCondition)
						{
							Shift(false);
						}

					}
				}

			}

			float standardBrakeForce = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 5.f), FVector2D(0.f, BrakeForce), FMath::Abs(GetSpeedKPH()));
			float powerBrake = FMath::Abs(RPM) * EngineBrakeCoefficient * (1 - Throttle) * xg * -velocityDirection;
			float wheelForce = CurrentTorque * xg * xd * (1.f / r);


			if (FMath::Abs(RPM) > (MaxRPM - rpmDelta))
			{
				wheelForce = 0.f;
			}

			if (!bInAir)
			{
				float brakeDirection = bUseBrakeAsReverse ? -1 : -velocityDirection;

				float brakesAsReverseForce = FMath::Abs(RPM) < MaxRPM ? BrakeForce : 0.f;

				float fb = bUseBrakeAsReverse ? brakesAsReverseForce : standardBrakeForce;
				float max_a = xg * xd * GetLinearVelocity() * WheelRadius * (60.f / 2.f * PI) * RPM_Multiplier;
				float saftey = 1.f;

				if (bUseBrakeAsReverse && (GetSpeedKPH() < -MaxSpeedInReverse))
				{
					fb = 0.f;
				}

				if (bUseDrivetrain)
				{					
					float interp = FMath::Lerp(SlipRestoreSpeed, SlipInitiateSpeed, Throttle);
					float power = GetAverageDrivetrainSlip() > 1.f ? 1.f / SlipPowerReduction : 1.f;
					EffectivePower = FMath::FInterpTo(EffectivePower, power, AsyncDeltaTime, interp);
					saftey = (FMath::Abs(max_a) / MaxRPM) < UpshiftRatio ? 1.f : 0.f;

				}

				float fres = ((wheelForce * TransmissionEfficiency * EffectivePower * Throttle * saftey) + (fb * Brakes * brakeDirection) + (powerBrake)) * 100.f;

				UAsyncTickFunctions::ATP_AddForce(PrimitiveComponent, fwd * fres, false);
				UAsyncTickFunctions::ATP_AddTorque(PrimitiveComponent, -rgt * fres * 10.f, false);

				if (DynamicPitchMomentMultiplier != 0)
				{
					UAsyncTickFunctions::ATP_AddTorque(PrimitiveComponent, -rgt * wheelForce * Throttle * 100.f * DynamicPitchMomentMultiplier, false);
				}
				if (DynamicPitchMomentMultiplier != 0)
				{
					UAsyncTickFunctions::ATP_AddTorque(PrimitiveComponent, -rgt * fb * Brakes * brakeDirection * 100.f * DynamicBrakeMomentMultiplier, false);
				}
			}

		}

		Drag = 0.5f * AirDensity * (FMath::Pow(GetLinearVelocity(), 2)) * CoefficientOfDrag * CrossSectionArea;
		UAsyncTickFunctions::ATP_AddForce(PrimitiveComponent, fwd * 100 * Drag * -velocityDirection, false);


		FVector _velocity = UKismetMathLibrary::InverseTransformDirection(PhysicsTransform, UAsyncTickFunctions::ATP_GetLinearVelocity(PrimitiveComponent));
		_velocity.Z = 0.f;


		float _rpm = xg * xd * GetLinearVelocity() * WheelRadius * (60.f / 2.f * PI) * RPM_Multiplier;
		float interpSpeed = 15.f;

		if (bUseDrivetrain)
		{
			if (!bInAir)
			{
				RPM = FMath::Clamp(FMath::FInterpTo(RPM, _rpm, DeltaTime, interpSpeed), -MaxRPM, MaxRPM);
			}
			else
			{
				float airGamma = FMath::Lerp(InAirSlowdownSpeed, InAirSpeedUp, Throttle);
				RPM = FMath::Clamp(FMath::FInterpTo(RPM, (Throttle-Brakes) * MaxRPM, DeltaTime, airGamma), 0.f, MaxRPM);
			}

		}
		else
		{
			RPM = FMath::Clamp(FMath::FInterpTo(RPM, _rpm, DeltaTime, interpSpeed), -MaxRPM, MaxRPM);
		}


		if (IsUpshifting)
		{
			UpshiftFrameCount++;

			if (UpshiftFrameCount * DeltaTime > ShiftDelay)
			{
				IsUpshifting = false;
			}
		}
		else
		{
			UpshiftFrameCount = 0;
		}

		if (IsDownshifting)
		{
			DownshiftFrameCount++;

			if (DownshiftFrameCount * DeltaTime > ShiftDelay)
			{
				IsDownshifting = false;
			}
		}
		else
		{
			DownshiftFrameCount = 0.f;
		}

	}
	else
	{
		//ARCADE POWERTRAIN
		float clampedTorque = FMath::GetMappedRangeValueClamped(FVector2D(MaxSpeed - 20.f, MaxSpeed), FVector2D(EngineTorque, 0.f), FMath::Abs(GetSpeedKPH()));
		CurrentTorque = clampedTorque;
		float clampedBrakeForce = BrakeForce;
		if (GetSpeedKPH() < 0 && GetSpeedKPH() < -MaxSpeedInReverse) { clampedBrakeForce = 0.f; }
		ResultantForce = clampedTorque * Xg * Xd * (1.f / WheelRadius);
		float dynamicForce = (Throttle * ResultantForce) - (Brakes * clampedBrakeForce);
		float fres = dynamicForce + (ExternalResistanceForce * -d);

		if (!bInAir)
		{
			UAsyncTickFunctions::ATP_AddForce(PrimitiveComponent, fres * fwd * 100.f, false);

			if (DynamicPitchMomentMultiplier != 0)
			{
				UAsyncTickFunctions::ATP_AddTorque(PrimitiveComponent, -rgt * dynamicForce * 100.f * DynamicPitchMomentMultiplier, false);
			}

		}
	}

	if (!bInAir)
	{
		float scaledTorque = FMath::GetMappedRangeValueClamped(FVector2D(3.f, 6.f), FVector2D(0.f, DriftTorque), FMath::Abs(GetLinearVelocity()));
		UAsyncTickFunctions::ATP_AddTorque(PrimitiveComponent, up * scaledTorque * SteeringInput * (FMath::Sign(GetLinearVelocity()) > 0 ? 1.f : -1.f), true);
	}

	PreviousLocalVelocity = LocalVelocity;
	FVector worldVelocity = UAsyncTickFunctions::ATP_GetLinearVelocity(PrimitiveComponent);
	LocalVelocity = UKismetMathLibrary::InverseTransformDirection(PhysicsTransform, worldVelocity);
	Speed = LocalVelocity.X * 0.01;

	Acceleration = (LocalVelocity - PreviousLocalVelocity) * 0.01 * (1.f/DeltaTime);


#pragma region Handbrake

	int frontHandbrakeCount = 0;
	int rearHandbrakeCount = 0;

	for (int i = 0; i < SuspensionComponents.Num(); i++)
	{
		if (SuspensionComponents[i] != nullptr)
		{
			EHandbrakeWheelType hbt = SuspensionComponents[i]->GetWheelHandbrake();

			if (hbt == EHandbrakeWheelType::Front && SuspensionComponents[i]->IsHandbrakeWheel())
			{
				frontHandbrakeCount++;
			}

			if (hbt == EHandbrakeWheelType::Rear && SuspensionComponents[i]->IsHandbrakeWheel())
			{
				rearHandbrakeCount++;
			}
		}

		bHandbrakeActive = (frontHandbrakeCount > 0 || rearHandbrakeCount > 0) && bHandbrakeInput;
		
		if (!bInAir)
		{
			if (bHandbrakeActive)
			{
				FVector planarVelocity = FVector
				(
					worldVelocity.X,
					worldVelocity.Y,
					0.f
				);

				FVector planarUdirection = planarVelocity;
				planarUdirection.Normalize();

				float hbf = HandbrakeStrength * (bHandbrakeInput? 1.f : 0.f);
				FVector2D hSpeedIn(0.f, 1.f);
				FVector2D hSpeedOut(0.f, hbf);
				float mapped_hbf = FMath::GetMappedRangeValueClamped(hSpeedIn, hSpeedOut, FMath::Abs(LocalVelocity.X));
				HandbrakeForce = mapped_hbf;

				UAsyncTickFunctions::ATP_AddForce(PrimitiveComponent, mapped_hbf * 100.f * -planarUdirection,false);

			}
		}
	}

#pragma endregion

#pragma region AirController


	FVector torque = FVector(
		(up * YawStrength * YawInput) + (rgt * PitchStrength * PitchInput) + (fwd * RollStrength * RollInput)
	);

	if (SuspensionComponents.Num() > 0)
	{
		int airborneCount = 0;
		for (USuspensionComponent* s : SuspensionComponents)
		{
			if (s->IsInAir())
			{
				airborneCount++;
			}
		}

		if (InAirWheelCount <= airborneCount)
		{
			UAsyncTickFunctions::ATP_AddTorque(PrimitiveComponent, torque, true);
		}
	}
	else
	{
		UAsyncTickFunctions::ATP_AddTorque(PrimitiveComponent, torque, true);
	}
#pragma endregion

	
	if (AntiGravity != nullptr)
	{
			if (bAntiGravityEnabled)
			{
				if(bInAir)
				{
					AntiGravityOffContactTime = AntiGravityOffContactTime + AsyncDeltaTime;
				}
				else
				{
					AntiGravityOffContactTime = 0;
				}

				bAntiGravityActive = (AntiGravityOffContactTime < AntiGravityOffContactThreshold);
				if ( bAntiGravityActive || bOverrideAntiGravity)
				{
					float z_base = FMath::Clamp(1.f - GetPhysicsTransform().GetRotation().GetUpVector().Dot(FVector(0.f, 0.f, 1.f)), -1.f, 1.f);
					float x_base = AntiGravity->GetUpVector().Dot(GetPhysicsTransform().GetRotation().GetForwardVector());
					float y_base = AntiGravity->GetUpVector().Dot(GetPhysicsTransform().GetRotation().GetRightVector());
					float mass = AntiGravityMass + AddedMass;

					FVector zForce = z_base * mass * GetPhysicsTransform().GetRotation().GetUpVector() * -GravityStrength * AntiGravityZStrength;
					FVector xForce = x_base * mass * GetPhysicsTransform().GetRotation().GetForwardVector() * GravityStrength * AntiGravityXStrength;
					FVector yForce = y_base * mass * GetPhysicsTransform().GetRotation().GetRightVector() * GravityStrength * AntiGravityYStrength;

					//Even though up, forward and right are mutually perpendicular - split this for neatness and futureproofing.
					UAsyncTickFunctions::ATP_AddForce(
						PrimitiveComponent,
						zForce,
						false,
						NAME_None
					);

					UAsyncTickFunctions::ATP_AddForce(
						PrimitiveComponent,
						xForce,
						false,
						NAME_None
					);

					UAsyncTickFunctions::ATP_AddForce(
						PrimitiveComponent,
						yForce,
						false,
						NAME_None
					);
				}
			}		
	}

	if (bUseSteerLean)
	{
		if (LeanSteerSuspension != nullptr)
		{
			if (!bInAir)
			{
				float leanAlpha = FVector::DotProduct(LeanSteerSuspension->GetHitResult().ImpactNormal, up);
				FVector2D leanIn = SteerLeanMapping;
				FVector2D leanOut = FVector2D(0.f, SteerLeanCoefficient);
				float leanScale = FMath::GetMappedRangeValueClamped(leanIn, leanOut, leanAlpha);

				FVector2D speedIn = FVector2D(0.f, SteerLeanMinSpeed);
				FVector2D outMapping = FVector2D(0.f, 1.f);
				float leanStrength = FMath::GetMappedRangeValueClamped(speedIn, outMapping, GetSpeedKPH());
				UAsyncTickFunctions::ATP_AddTorque(PrimitiveComponent, SteeringInput * -1.f * leanScale * leanStrength * fwd, true);

			}
		}
	}
}


void AForgeGroundVehicle::AnimateSteering()
{
	float sensitivity = bUseSensitivityClamp ? FMath::GetMappedRangeValueClamped(SteeringSensitivityClamp.InputRange, SteeringSensitivityClamp.OutputRange, GetSpeedKPH()) : SteeringSensitivity;
	for (int i = 0; i < SuspensionComponents.Num(); i++)
	{
		if (SuspensionComponents[i]->IsSteeringWheel())
		{
			SuspensionComponents[i]->SetSteeringInput(SteeringInput, sensitivity);
		}
	}
}

void AForgeGroundVehicle::ServerStateSync(bool updatePhysics)
{
	if (updatePhysics)
	{
		ServerState.ServerLinearVelocity = UAsyncTickFunctions::ATP_GetLinearVelocity(PrimitiveComponent);
		ServerState.ServerAngularVelocity = UAsyncTickFunctions::ATP_GetAngularVelocity(PrimitiveComponent);
		ServerState.ServerTransform = UAsyncTickFunctions::ATP_GetTransform(PrimitiveComponent);
	}

	ServerState.ServerRPM = RPM;
	ServerState.ServerCurrentGearRatio = Xg;
	ServerState.ServerCurrentGear = CurrentGear;
	ServerState.ServerThrottle = Throttle;
	ServerState.ServerBrakes = Brakes;
	ServerState.ServerSteering = SteeringInput;
	ServerState.ServerSpeed = Speed;
	ServerState.ServerIsUpshifting = IsUpshifting;
	ServerState.ServerIsDownshifting = IsDownshifting;
}


void AForgeGroundVehicle::ClientStateSync(bool updatePhysics)
{
	if (updatePhysics)
	{
		UAsyncTickFunctions::ATP_SetLinearVelocity(PrimitiveComponent, ServerState.ServerLinearVelocity, false);
		UAsyncTickFunctions::ATP_SetAngularVelocityInRadians(PrimitiveComponent, ServerState.ServerAngularVelocity, false);
	}
	RPM = ServerState.ServerRPM;
	Xg = ServerState.ServerCurrentGearRatio;
	CurrentGear = ServerState.ServerCurrentGear;
	Throttle = ServerState.ServerThrottle;
	Brakes = ServerState.ServerBrakes;
	SteeringInput = ServerState.ServerSteering;
	Speed = ServerState.ServerSpeed;
	IsUpshifting = ServerState.ServerIsUpshifting;
	IsDownshifting = ServerState.ServerIsDownshifting;
}


float AForgeGroundVehicle::GetRPM()
{
	return RPM;
}

bool AForgeGroundVehicle::IsGearboxUpshifting()
{
	return IsUpshifting;
}

bool AForgeGroundVehicle::IsGearboxDownshifting()
{
	return IsDownshifting;
}

int AForgeGroundVehicle::GetCurrentGear()
{
	return CurrentGear;
}

FTransform AForgeGroundVehicle::GetPhysicsTransform()
{
	return PhysicsTransform;
}

bool AForgeGroundVehicle::GetIsHandbrakeActive()
{
	return bHandbrakeActive;
}


float AForgeGroundVehicle::GetSideSlipAngle()
{
	return FMath::RadiansToDegrees(FMath::Atan(UKismetMathLibrary::SafeDivide(GetVelocityVector().Y, GetVelocityVector().X)));
}

float AForgeGroundVehicle::GetSideSlipAngleGT()
{
	return FMath::RadiansToDegrees(FMath::Atan(UKismetMathLibrary::SafeDivide(LocalVelocityGT.Y, LocalVelocityGT.X)));
}

float AForgeGroundVehicle::GetHandbrakeStrength()
{
	return HandbrakeForce;
}

UPrimitiveComponent* AForgeGroundVehicle::GetPrimitiveRoot()
{
	return PrimitiveComponent;
}

bool AForgeGroundVehicle::IsAntiGravityActive()
{
	return bAntiGravityActive;
}

float AForgeGroundVehicle::GetLongitudinalG()
{
	return (Acceleration.X/9.81);
}

float AForgeGroundVehicle::GetLateralG()
{
	return (Acceleration.Y / 9.81);
}

float AForgeGroundVehicle::GetVerticalG()
{
	return (Acceleration.Z / 9.81);
}

FVector AForgeGroundVehicle::GetLocationVelocity()
{
	return LocationVelocity;
}

float AForgeGroundVehicle::GetAverageDrivetrainSlip()
{
	return AveragedSlip;
}

float AForgeGroundVehicle::GetAveragedSlipGamma()
{
	return AveragedSlipGamma;
}

bool AForgeGroundVehicle::IsSlipping()
{
	return AveragedSlip > 1.f;
}

float AForgeGroundVehicle::GetAngularVelocity()
{
	return AngularVelocity;
}

float AForgeGroundVehicle::GetAngularAcceleration()
{
	return AngularAcceleration;
}

float AForgeGroundVehicle::GetDrivetrainWheelCount()
{
	return DrivetrainWheelCount;
}

TArray<USuspensionComponent*> AForgeGroundVehicle::GetSuspension()
{
	return SuspensionComponents;
}

void AForgeGroundVehicle::Shift(bool bUpshift)
{
	if (bUpshift)
	{
		if (CurrentGear + 1 < GearRatios.Num() && (!IsUpshifting || IsDownshifting))
		{
			IsUpshifting = true;
			CurrentGear++;
		}
	}
	else
	{
		if (CurrentGear - 1 >= 0 && (IsUpshifting || !IsDownshifting))
		{
			IsDownshifting = true;
			CurrentGear--;
		}
	}
}

void AForgeGroundVehicle::Upshift()
{
	Shift(true);
}

void AForgeGroundVehicle::Downshift()
{
	Shift(false);
}

void AForgeGroundVehicle::OverrideRPM(float newRPM)
{
	RPM = newRPM;
}

void AForgeGroundVehicle::OverrideAntiGravityState(bool bOn)
{
	bOverrideAntiGravity = bOn;
}

void AForgeGroundVehicle::RTWakeRigidBodies()
{
	PrimitiveComponent->WakeAllRigidBodies();
}

void AForgeGroundVehicle::SetControllable(bool inputEnabled)
{
	bInputEnabled = inputEnabled;
	ServerSetControllable(bInputEnabled);
}

void AForgeGroundVehicle::ServerSetControllable_Implementation(bool inputEnabled)
{
	bInputEnabled = inputEnabled;
}

bool AForgeGroundVehicle::ServerSetControllable_Validate(bool inputEnabled)
{
	return true;
}

void AForgeGroundVehicle::SetArcadeMaxSpeed(float newMaxSpeed)
{
	MaxSpeed = newMaxSpeed;
}


#pragma region Settors

void AForgeGroundVehicle::SetThrottleInput(float value)
{
	Throttle = (IsUpshifting ? 0 : value) * GetControllableValue();
	ServerSetThrottleInput(Throttle);
}


void AForgeGroundVehicle::ServerSetThrottleInput_Implementation(float value)
{
	Throttle = value * GetControllableValue();
}


bool AForgeGroundVehicle::ServerSetThrottleInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}


void AForgeGroundVehicle::ApplyHandbrakeInput(bool value)
{
	bHandbrakeInput = value;
	ServerApplyHandbrakeInput(bHandbrakeInput);
}


void AForgeGroundVehicle::ServerApplyHandbrakeInput_Implementation(bool value)
{
	bHandbrakeInput = value;
}


bool AForgeGroundVehicle::ServerApplyHandbrakeInput_Validate(bool value)
{
	return true;
}


void AForgeGroundVehicle::SetBrakeInput(float value)
{
	Brakes = value * GetControllableValue();
	ServerSetBrakeInput(Brakes);
}


void AForgeGroundVehicle::ServerSetBrakeInput_Implementation(float value)
{
	Brakes = value * GetControllableValue();
}


bool AForgeGroundVehicle::ServerSetBrakeInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}


float AForgeGroundVehicle::GetProcessedSteeringInput(float value)
{
	float processedInput = 0.f;
	switch (SteeringInputClamp)
	{
	case ESpeedClamp::NoClamp:
		processedInput = value;
		break;

	case ESpeedClamp::Constant:
		processedInput = value * ConstantSteeringClamp;
		break;

	case ESpeedClamp::Linear:
		processedInput = value * FMath::GetMappedRangeValueClamped(LinearSteeringInputClamp.InputRange, LinearSteeringInputClamp.OutputRange, GetSpeedKPH());
		break;

	case ESpeedClamp::Curve:
		if (CurveSteeringInputClamp != nullptr)
		{
			processedInput = value * CurveSteeringInputClamp->GetFloatValue(GetSpeedKPH());
		}
		else
		{
			processedInput = value;
		}
		break;
	}

	return processedInput * GetControllableValue();
}

float AForgeGroundVehicle::GetControllableValue()
{
	return bInputEnabled ? 1.f : 0.f;
}

void AForgeGroundVehicle::SetSteeringInput(float value)
{
	SteeringInput = GetProcessedSteeringInput(value);
	ServerSetSteeringInput(SteeringInput);

	/*
	float sensitivity = bUseSensitivityClamp ? FMath::GetMappedRangeValueClamped(SteeringSensitivityClamp.InputRange, SteeringSensitivityClamp.OutputRange, GetSpeedKPH()) : SteeringSensitivity;
	for (int i = 0; i < SuspensionComponents.Num(); i++)
	{
		if (SuspensionComponents[i]->IsSteeringWheel())
		{
			SuspensionComponents[i]->SetSteeringInput(SteeringInput,sensitivity);
		}
	}	
	*/
	
}

void AForgeGroundVehicle::ServerSetSteeringInput_Implementation(float value)
{
	SteeringInput = value * GetControllableValue();
}

bool AForgeGroundVehicle::ServerSetSteeringInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

void AForgeGroundVehicle::Jump(float amount)
{
	if (!bInAir)
	{
		FVector up = PhysicsTransform.GetRotation().GetUpVector();
		UAsyncTickFunctions::ATP_AddImpulse(PrimitiveComponent, up * amount, true);
	}
}


void AForgeGroundVehicle::SetAirPitchInput(float value)
{
	PitchInput = value * GetControllableValue();
	ServerSetAirPitchInput(value);
}

void AForgeGroundVehicle::ServerSetAirPitchInput_Implementation(float value)
{
	PitchInput = value * GetControllableValue();
}


bool AForgeGroundVehicle::ServerSetAirPitchInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}


void AForgeGroundVehicle::SetAirRollInput(float value)
{
	RollInput = value * GetControllableValue();
	ServerSetAirRollInput(value);
}

void AForgeGroundVehicle::ServerSetAirRollInput_Implementation(float value)
{
	RollInput = value * GetControllableValue();
}

bool AForgeGroundVehicle::ServerSetAirRollInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

void AForgeGroundVehicle::SetAirYawInput(float value)
{
	YawInput = value * GetControllableValue();
	ServerSetAirYawInput(value);
}

void AForgeGroundVehicle::ServerSetAirYawInput_Implementation(float value)
{
	YawInput = value * GetControllableValue();
}


bool AForgeGroundVehicle::ServerSetAirYawInput_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}


#pragma region ReplicationInput
///

void AForgeGroundVehicle::SetExtReplicationInput1(float value)
{
	ExtReplicationInput1 = value;
	ServerSetExtReplicationInput1(ExtReplicationInput1);

}

void AForgeGroundVehicle::ServerSetExtReplicationInput1_Implementation(float value)
{
	ExtReplicationInput1 = value;
}

bool AForgeGroundVehicle::ServerSetExtReplicationInput1_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

float AForgeGroundVehicle::GetExtReplicationInput1()
{
	return ExtReplicationInput1;
}

///

void AForgeGroundVehicle::SetExtReplicationInput2(float value)
{
	ExtReplicationInput2 = value;
	ServerSetExtReplicationInput2(ExtReplicationInput2);

}

void AForgeGroundVehicle::ServerSetExtReplicationInput2_Implementation(float value)
{
	ExtReplicationInput2 = value;
}

bool AForgeGroundVehicle::ServerSetExtReplicationInput2_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

float AForgeGroundVehicle::GetExtReplicationInput2()
{
	return ExtReplicationInput2;
}

///

void AForgeGroundVehicle::SetExtReplicationInput3(float value)
{
	ExtReplicationInput3 = value;
	ServerSetExtReplicationInput3(ExtReplicationInput3);

}

void AForgeGroundVehicle::ServerSetExtReplicationInput3_Implementation(float value)
{
	ExtReplicationInput3 = value;
}

bool AForgeGroundVehicle::ServerSetExtReplicationInput3_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

float AForgeGroundVehicle::GetExtReplicationInput3()
{
	return ExtReplicationInput3;
}

///

void AForgeGroundVehicle::SetExtReplicationInput4(float value)
{
	ExtReplicationInput4 = value;
	ServerSetExtReplicationInput4(ExtReplicationInput4);

}

void AForgeGroundVehicle::ServerSetExtReplicationInput4_Implementation(float value)
{
	ExtReplicationInput4 = value;
}

bool AForgeGroundVehicle::ServerSetExtReplicationInput4_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

float AForgeGroundVehicle::GetExtReplicationInput4()
{
	return ExtReplicationInput4;
}

///

void AForgeGroundVehicle::SetExtReplicationInput5(float value)
{
	ExtReplicationInput5 = value;
	ServerSetExtReplicationInput5(ExtReplicationInput5);

}

void AForgeGroundVehicle::ServerSetExtReplicationInput5_Implementation(float value)
{
	ExtReplicationInput5 = value;
}

bool AForgeGroundVehicle::ServerSetExtReplicationInput5_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

float AForgeGroundVehicle::GetExtReplicationInput5()
{
	return ExtReplicationInput5;
}

///

void AForgeGroundVehicle::SetExtReplicationInput6(float value)
{
	ExtReplicationInput6 = value;
	ServerSetExtReplicationInput5(ExtReplicationInput6);

}

void AForgeGroundVehicle::ServerSetExtReplicationInput6_Implementation(float value)
{
	ExtReplicationInput6 = value;
}

bool AForgeGroundVehicle::ServerSetExtReplicationInput6_Validate(float value)
{
	return FMath::Abs(value) <= 1;
}

float AForgeGroundVehicle::GetExtReplicationInput6()
{
	return ExtReplicationInput6;
}

#pragma endregion

void AForgeGroundVehicle::SetEngineTorque(float newTorque)
{
	EngineTorque = newTorque;
}

void AForgeGroundVehicle::SetCurrentGearRatio(float newGearRatio)
{
	Xg = newGearRatio;
}

void AForgeGroundVehicle::SetGear(int32 newGear)
{
	if (newGear < 0 || newGear > (GearRatios.Num()-1) )
	{ 
		return;
	}
	else
	{
		CurrentGear = newGear;
	}

}

void AForgeGroundVehicle::SetDifferentialRatio(float newDifferentialRatio)
{
	Xd = newDifferentialRatio;
}

void AForgeGroundVehicle::SetPhysicsWheelRadius(float newRadius)
{
	WheelRadius = newRadius;
}

void AForgeGroundVehicle::SetExternalResistanceForce(float force)
{
	ExternalResistanceForce = force;
}

#pragma endregion

#pragma region Gettors



float AForgeGroundVehicle::GetSpeedKPH()
{

	return Speed * 3.6f;
}

float AForgeGroundVehicle::GetLinearVelocity()
{
	return UKismetMathLibrary::InverseTransformDirection(PhysicsTransform, UAsyncTickFunctions::ATP_GetLinearVelocity(PrimitiveComponent)).X * 0.01f;
}

FVector AForgeGroundVehicle::GetVelocityVector()
{
	return UKismetMathLibrary::InverseTransformDirection(PhysicsTransform, UAsyncTickFunctions::ATP_GetLinearVelocity(PrimitiveComponent)) * 0.01f;
}

float AForgeGroundVehicle::GetThrottleInput()
{
	return Throttle;
}

float AForgeGroundVehicle::GetBrakesInput()
{
	return Brakes;
}

float AForgeGroundVehicle::GetSteeringInput()
{
	return SteeringInput;
}

bool AForgeGroundVehicle::GetHandbrakeInput()
{
	return bHandbrakeInput;
}

void AForgeGroundVehicle::SetBrakesEnabled(bool bEnabled)
{
	//float r = BrakeForce;
	BrakeForce = bEnabled ? BrakeForceDuplicate : 0.f;

}

void AForgeGroundVehicle::SetEngineTorqueEnabled(bool bEnabled, float tempTorque)
{
	EngineTorque = bEnabled ? EngineTorqueDuplicate : tempTorque;
}

void AForgeGroundVehicle::SetInAir(bool bAirborne)
{
	bInAir = bAirborne;
}


float AForgeGroundVehicle::GetEngineTorque()
{
	return EngineTorque;
}

float AForgeGroundVehicle::GetEngineCurrentTorque()
{
	return CurrentTorque;
}

float AForgeGroundVehicle::GetCurrentGearRatio()
{
	return Xg;
}

float AForgeGroundVehicle::GetDifferentialRatio()
{
	return Xd;
}

float AForgeGroundVehicle::GetPhysicsWheelRadius()
{
	return WheelRadius;
}

float AForgeGroundVehicle::GetExternalResistanceForce()
{
	return ExternalResistanceForce;
}

#pragma endregion