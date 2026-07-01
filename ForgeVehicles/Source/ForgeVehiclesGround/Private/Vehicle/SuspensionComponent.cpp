//Copyright 2025 P.Kallisto 

#include "Vehicle/SuspensionComponent.h"
#include "AsyncTickFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/StaticMesh.h"

#include "Physics/Experimental/PhysScene_Chaos.h"

//5.4
#include "Modules/ModuleInterface.h"
#include "PhysicsInterfaceDeclaresCore.h"

// Sets default values for this component's properties
USuspensionComponent::USuspensionComponent()
{
	// Set this component to be initialized when the game Starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	ComponentTags.Add(FName("SuspensionComponent"));
	/*
#if WITH_EDITOR
	WheelMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wheel Mesh"));
	//WheelMeshComponent->SetupAttachment();
	WheelMeshComponent->SetupAttachment(this);
	WheelMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);


#endif
	*/
}

void USuspensionComponent::ConstructWheelMeshComponent()
{

}

// Called when the game starts
void USuspensionComponent::BeginPlay()
{
	Super::BeginPlay();

	VehiclePrimitiveComponent = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	bool bValidChild = GetChildComponent(0) != nullptr;

	if (!bInheritMeshFromComponent || !bValidChild)
	{
		InstantiateWheel();

		if (VehiclePrimitiveComponent == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("FTSuspension: No valid primitive root component! in vehicle found"));
		}


		WheelMeshComponent->SetStaticMesh(WheelMesh);
	}
	else
	{
		TArray<USceneComponent*,FDefaultAllocator> wheels;
		GetChildrenComponents(true, wheels);

		if (wheels.Num() > 0)
		{
			if (Cast<UStaticMeshComponent>(GetChildComponent(0)) != nullptr)
			{
				WheelMeshComponent = Cast<UStaticMeshComponent>(GetChildComponent(0));
				WheelMeshComponent->SetIsReplicated(false);
				WheelMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				if (bRotateWheel) WheelMeshComponent->SetWorldScale3D(this->GetComponentScale());
			}
			else
			{
				InstantiateWheel();
			}
		}
		else
		{
			InstantiateWheel();
		}
	}



	if (bRotateWheel)
	{
		WheelMeshComponent->SetRelativeRotation((FRotator(0.f, 180.f, 0.f)));
	}

	if (WheelMeshComponent->GetStaticMesh() != nullptr)
	{
		WheelRadius = WheelMeshComponent->GetStaticMesh()->GetBoundingBox().GetSize().Z / 2.f;
	}
	else
	{
		WheelRadius = Radius;
	}

	SweepShape = FCollisionShape::MakeSphere(WheelRadius);

	Direction = this->GetRelativeRotation().Yaw;


	WheelMeshComponent->SetVisibility(!bHideWheelMesh);

	if (bUseWithDrivetrain)
	{
		bUseIdleLock = false;
	}

//	UE_LOG(LogTemp, Warning, TEXT("Wheel Radius: %f"), WheelRadius);

}


void USuspensionComponent::SetSteeringInput(float inSteering, float sensitivity)
{
	SteeringInput = inSteering;
	SensitvityInput = sensitivity;	

	//ClientSetSteeringInput(SteeringInput, SensitvityInput);
}

void USuspensionComponent::SimulateInternalAnimation(float deltaTime, UStaticMeshComponent* wheelMeshComponent)
{
	if(wheelMeshComponent==nullptr) 
	{
		return;
	}
	
	FTransform t = GetOwner()->GetActorTransform();
	Previous_UAS_Location = Current_UAS_Location;
	Current_UAS_Location = t.GetLocation();

	FVector worldlocationVelocity = (Current_UAS_Location - Previous_UAS_Location) * (1.f / 100.f) * (1.f / deltaTime);
	LocationVelocity = UKismetMathLibrary::InverseTransformDirection(t, worldlocationVelocity).X;

	float v = LocationVelocity;
	float angularVelocity = (v * 100.f) / WheelRadius * -1.f;

	//If the frame rate is higher than 60, scale the rotation accordingly to compensate these extra rotations added every frame
	float timeScalar = 60.f / FMath::Pow(deltaTime, -1.f);

	FRotator angularRotator = FRotator( (angularVelocity - BurnoutRotation) * timeScalar, 0.f, 0.f);
	angularAnimRotator = angularRotator;

	int d = 1;
	if (bRotateWheel)
	{
		d = -1;
	}


	
	wheelMeshComponent->AddLocalRotation(( angularRotator * WheelAngularVelocityMultiplier * HandbrakeVelocityMultiplier * d));

	//Vertical displacement may need to be handled as well

}

void USuspensionComponent::UpdateAnimationState(bool bInUseAnimationSystem, float inLocationVelocity)
{
	bUseAnimationSystem = bInUseAnimationSystem;
	LocationVelocity = inLocationVelocity;
}
/*
void USuspensionComponent::ClientSetSteeringInput_Implementation(float inSteering, float sensitivity)
{
	SteeringInput = inSteering;
	SensitvityInput = sensitivity;
}
*/
//Set steering Sensitivity
void USuspensionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USuspensionComponent, ServerSteeringRotation);
}

void USuspensionComponent::UpdateSteeringGeometry(float DeltaTime)
{
	float steeringSpeed = SteeringInput == 0 ? SteeringReleaseSpeed : SensitvityInput;
	float r = FMath::FInterpTo(GetRelativeRotation().Yaw, (SteeringInput * MaxSteeringAngle), GameTime, steeringSpeed);
	SteeringRotation = r;
	FRotator baseRotation = this->GetRelativeRotation();


	if (GetOwner()->HasAuthority())
	{
		ServerSteeringRotation = r;
		//FRotator rotator = FRotator(baseRotation.Pitch, r, baseRotation.Roll);
		FRotator rotator = bAllowFullRotation ? FRotator(baseRotation.Pitch, r, baseRotation.Roll) : FRotator(0.f, r, 0.f);
		this->SetRelativeRotation(rotator);
	}
	else
	{
		ClientTimeSinceUpdate += DeltaTime;

		if (ClientTimeBetweenLastUpdates < KINDA_SMALL_NUMBER)
		{
			//return;
		}

		float lerpRatio = ClientTimeSinceUpdate / ClientTimeBetweenLastUpdates;
		float smooth_q = FMath::LerpStable(ClientStartSteeringRotation, ServerSteeringRotation, lerpRatio);

		//FRotator qs = FRotator(baseRotation.Pitch, ServerSteeringRotation, baseRotation.Roll);
		FRotator qs = bAllowFullRotation ? FRotator(baseRotation.Pitch, ServerSteeringRotation, baseRotation.Roll) : FRotator(0.f, ServerSteeringRotation, 0.f);
		this->SetRelativeRotation(qs);
	}

}

void USuspensionComponent::SetHandbrakeInput(bool bInHandbrakeOn)
{
	bHandbrakeInput = bInHandbrakeOn;
}


void USuspensionComponent::OnRep_Steering()
{
	ClientTimeBetweenLastUpdates = ClientTimeSinceUpdate;
	ClientTimeSinceUpdate = 0;

	float steeringSpeed = SteeringInput == 0 ? SteeringReleaseSpeed : SensitvityInput;
	float r = FMath::FInterpTo(GetRelativeRotation().Yaw, SteeringInput * MaxSteeringAngle, GameTime, steeringSpeed);
	//float r = SteeringInput * MaxSteeringAngle;
	ClientStartSteeringRotation = r;
	FRotator baseRotation = this->GetRelativeRotation();
	FRotator qs = bAllowFullRotation ? FRotator(0.f, ClientStartSteeringRotation, 0.f) : FRotator(baseRotation.Pitch,ClientStartSteeringRotation, baseRotation.Roll);
	//this->SetRelativeRotation(qs);
	//Maybe update relative rotation?
}

void USuspensionComponent::InstantiateWheel()
{
	WheelMeshComponent = NewObject<UStaticMeshComponent>(GetOwner());

	if (WheelMeshComponent != nullptr)
	{
		WheelMeshComponent->RegisterComponent();
		WheelMeshComponent->SetWorldLocation(this->GetComponentLocation());
		WheelMeshComponent->SetWorldRotation(this->GetComponentRotation());

		WheelMeshComponent->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);

		WheelMeshComponent->SetIsReplicated(false);

		WheelMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (bRotateWheel) WheelMeshComponent->SetWorldScale3D(this->GetComponentScale());
		if (WheelMeshComponent) WheelMeshComponent->SetStaticMesh(WheelMesh);
	}
}


bool USuspensionComponent::IsSteeringWheel()
{
	return bIsSteeringWheel;
}


void USuspensionComponent::UpdateTick(float DeltaTime)
{	
	GameTime = DeltaTime;

	float deformation = 0.05f * WheelRadius;
	deformation = 0.f;
	wheelAnimZ = 0.f;

	if (bSingleRaycast)
	{
		wheelAnimZ = -CurrentLength + (0.5f * WheelRadius * 2.f) - deformation;
		WheelMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, -CurrentLength + (0.5f * WheelRadius * 2.f) - deformation));
		WheelRelativeOffset = -CurrentLength + (0.5f * WheelRadius * 2.f) - deformation;
	}
	else
	{
		wheelAnimZ = -CurrentLength - deformation;
		WheelMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, -CurrentLength - deformation + WheelZOffset));
		WheelRelativeOffset = -CurrentLength - deformation;
	}

	UpdateSteeringGeometry(DeltaTime);


#pragma region DynamicCamber

#pragma endregion


	float v = LinearVelocity.X;
	float angularVelocity =  (v * 100.f) / WheelRadius * -1.f;

	//If the frame rate is higher than 60, scale the rotation accordingly to compensate these extra rotations added every frame
	float timeScalar = 60.f / FMath::Pow(DeltaTime, -1.f);

	FRotator angularRotator = FRotator((angularVelocity-BurnoutRotation)*timeScalar, 0.f, 0.f);
	angularAnimRotator = angularRotator;

	int d = 1;
	if (bRotateWheel)
	{
		d = -1;
	}

	if (!bIsIdleLocked && !(bHandbrakeInput && bHandbrakeWheel))
	{
		if (!bUseAnimationSystem)
		{
			WheelLocalRotation = angularRotator * WheelAngularVelocityMultiplier * HandbrakeVelocityMultiplier * d;
			WheelMeshComponent->AddLocalRotation((angularRotator * WheelAngularVelocityMultiplier * HandbrakeVelocityMultiplier * d));
		}
	}

	if (bIsIdleLocked)
	{
		FRotator staticRotator = FRotator(-BurnoutRotation * timeScalar, 0.f, 0.f);
		WheelLocalRotation = staticRotator * WheelAngularVelocityMultiplier * HandbrakeVelocityMultiplier * d;
	}

	else if ((bHandbrakeInput && bHandbrakeWheel))
	{
		WheelMeshComponent->AddLocalRotation(FRotator(0.f));
		WheelLocalRotation = FRotator::ZeroRotator;
	}


	if (bDebug)
	{
		if (Hit.bBlockingHit)
		{
			DrawDebugLine(GetWorld(), Start, Hit.ImpactPoint, FColor::Green, false, -1.f, 1, 2.f);
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 8.f, 10, FColor::Red, false, -1.f, 2.f);
		}
	}


}

void USuspensionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}


void USuspensionComponent::StaticallyRenderWheel(UStaticMeshComponent* MeshComponent)
{
	if (!ensure(MeshComponent != nullptr))return;

	FVector min = FVector(0);
	FVector max = FVector(0);
	MeshComponent->GetLocalBounds(min, max);
	float lRadius = max.Z / 2.f;
	MeshComponent->SetRelativeLocation(FVector(0.f, 0.f, -ExtensionLength + (0.5f * lRadius * 2.f) +  ((9810.f/Stiffness)) ));
	//WheelStaticRadius = lRadius;
}

void USuspensionComponent::StaticWheelCallback(UStaticMeshComponent* MeshComponent)
{
	MeshComponent->DestroyComponent();
}

void USuspensionComponent::SetWheelScale(float inScale)
{
	if (WheelMeshComponent->GetStaticMesh() != nullptr)
	{
		WheelRadius = WheelMeshComponent->GetStaticMesh()->GetBoundingBox().GetSize().Z / 2.f;
	}
	else
	{
		WheelRadius = Radius;
	}

	SweepShape = FCollisionShape::MakeSphere(WheelRadius * inScale);
	WheelMeshComponent->SetRelativeScale3D(FVector(inScale));
}

void USuspensionComponent::RecalculateWheelRadius()
{
	if (WheelMeshComponent->GetStaticMesh() != nullptr)
	{
		WheelRadius = WheelMeshComponent->GetStaticMesh()->GetBoundingBox().GetSize().Z / 2.f;
	}

	else
	{
		WheelRadius = Radius;
	}

	SweepShape = FCollisionShape::MakeSphere(WheelRadius);
}

void USuspensionComponent::UpdatePhysics(float DeltaTime, float transmittedTorque, float slipForward, float slipBackwards, float throttle, float brakes)
{
	AsyncTime = DeltaTime;

	//IdleLock only for animation

	if (FMath::Abs((LinearVelocity.X * 3.6f)) < IdleLockSpeedThreshold)
	{
		if (!bIsIdleLocked)
		{
			IdleLockCount += DeltaTime;
		}

		if ((IdleLockCount > IdleLockTime) && bUseIdleLock)
		{
			bIsIdleLocked = true;
			IdleLockCount = 612.f;
		}
	}
	else
	{
		IdleLockCount = 0;
		bIsIdleLocked = false;
	}

	FTransform vehicleTransform = UAsyncTickFunctions::ATP_GetTransform(VehiclePrimitiveComponent);
	PhysicsTransform = GetRelativeTransform() * vehicleTransform;
	Start = PhysicsTransform.GetLocation();
	FVector up = PhysicsTransform.GetRotation().GetUpVector();
	FVector right = PhysicsTransform.GetRotation().GetRightVector();
	FVector fwd = PhysicsTransform.GetRotation().GetForwardVector();

	FVector worldPointVelocity = UAsyncTickFunctions::ATP_GetLinearVelocityAtPoint(VehiclePrimitiveComponent, PhysicsTransform.GetLocation());

	LinearVelocity = UKismetMathLibrary::InverseTransformDirection(PhysicsTransform, worldPointVelocity) * 0.01f;

#pragma region QueryParams

	FCollisionQueryParams queryparam = FCollisionQueryParams::DefaultQueryParam;
	const FName traceTag("Trace");

	queryparam.TraceTag = traceTag;
	queryparam.AddIgnoredActor(GetOwner());
	queryparam.AddIgnoredComponent(WheelMeshComponent);
	queryparam.bReturnPhysicalMaterial = true;


#pragma endregion



#pragma region Raycast

#pragma region Params

	float length = ExtensionLength;

#pragma endregion

#pragma region PureSphereTrace

	//GetWorld()->SweepSingleByChannel(Hit, Start, Start + (-up * length), FQuat::Identity, ECC_WorldStatic, SweepShape, queryparam);
	GetWorld()->SweepSingleByChannel(Hit, Start, Start + (-up * length), FQuat::Identity, CollisionChannel, SweepShape, queryparam);

	FVector worldDirection = (PhysicsTransform.GetLocation() - Hit.ImpactPoint);
	worldDirection.Normalize();//optional
	ImpactNormal = UKismetMathLibrary::InverseTransformDirection(PhysicsTransform, worldDirection);
	ImpactDot = FMath::Abs(FVector::DotProduct(-up, ImpactNormal));

	//FVector localImpactPoint = UKismetMathLibrary::InverseTransformLocation(VehiclePhysicsBody->GetUnrealWorldTransform(), Hit.ImpactPoint);
	FVector localImpactPoint = UKismetMathLibrary::InverseTransformLocation(vehicleTransform, Hit.ImpactPoint);
		
	bool safteyCondition = FMath::Abs(ImpactNormal.Y) <= ImpactNormalTolerance && localImpactPoint.Z <= ImpactHeightTolerance;

	bSingleRaycast = false;

	if ( (Hit.bBlockingHit && safteyCondition && bHybridRaycast))
	{
		CurrentLength = (Start - Hit.Location).Size();
	}
	else
	{
		//Pure Linetrace//
		bSingleRaycast = true;
		length = ExtensionLength + WheelRadius + (0.125f * WheelRadius);
		//GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - (up * length), ECC_WorldStatic, queryparam);
		GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - (up * length), CollisionChannel, queryparam);
		CurrentLength = (Start - Hit.Location).Size();

		if (!Hit.bBlockingHit)
		{
			CurrentLength = length;
		}
	}


#pragma endregion



	float compression = 1 - (CurrentLength / length);
	OutCompression = compression;

	float velocity = (compression - PreviousCompression) / AsyncTime;

	float force = (compression * Stiffness) + (Damping * velocity);

	if (Hit.bBlockingHit || bOverrideContact)
	{
		if (!bZeroForceComponent)
		{
			UAsyncTickFunctions::ATP_AddForceAtPosition(VehiclePrimitiveComponent, PhysicsTransform.GetLocation(), force * 100.f * Hit.Normal);
		}

#pragma region TyrePhysics

		if (!bIsIdleLocked)
		{
			float sy_low = -LinearVelocity.Y;
			//try if v.x == 0->sy_low = 0.f;
			float lowLateralForce = sy_low * LowSpeedLateralForceScalar;
			float lateralForce = FMath::Clamp(sy_low * LateralForceScalar, -MaxLateralForce, MaxLateralForce);
			float interp = FMath::GetMappedRangeValueClamped(FVector2D(3.f, 6.f), FVector2D(0.f, 1.f), LinearVelocity.Size());
			float lateralForce_dynamic = FMath::Lerp(lowLateralForce, lateralForce, interp);
			FVector com = UAsyncTickFunctions::ATP_GetCoMPosition(VehiclePrimitiveComponent);
			FVector z1 = FVector(0.f, 0.f, UKismetMathLibrary::InverseTransformLocation(PhysicsTransform, PhysicsTransform.GetLocation()).Z);
			FVector z2 = FVector(0.f, 0.f, UKismetMathLibrary::InverseTransformLocation(PhysicsTransform, com).Z);
			float comDistOffset = FVector::Dist(z1, z2);

			FVector forceLocation = PhysicsTransform.GetLocation() + (up * comDistOffset) + (-up * ContactForceOffsetLocation);

			if (!bZeroForceComponent)
			{
				UAsyncTickFunctions::ATP_AddForceAtPosition(VehiclePrimitiveComponent, forceLocation, lateralForce_dynamic * 100.f * right);
			}
			
			if (ResistanceForceCoefficient != 0.f)
			{				
				float resistanceForce = FMath::Clamp(-LinearVelocity.X * ResistanceForceCoefficient, -MaxResistanceForce, MaxResistanceForce);

				if (!bZeroForceComponent)
				{
					UAsyncTickFunctions::ATP_AddForceAtPosition(VehiclePrimitiveComponent, forceLocation, resistanceForce * 100.f * fwd);
				}
			}

		}

#pragma endregion

	}


	if (bUseWithDrivetrain)
	{
		if (bDrivenWheel)
		{
			AppliedTorque = transmittedTorque * TorqueReceived;

			float deltaX = (ExtensionLength - CurrentLength) * (1.f / 100.f);
			float ffMax = deltaX * Stiffness * FrictionCoefficient;


			FrRatio = AppliedTorque / ffMax;
			float iFrRatio = 1.f / FrRatio;

			Gamma = FrRatio > 1.f;

			DynamicAngularVelocity = Gamma ? SlippingAngularVelocity : 1.f;
			DynamicLateralForce = Gamma ? SlippingLateralForceScalar : MaxLateralForceScalar;
			float slipInterp = FMath::Lerp(slipBackwards, slipForward, throttle);

			ThrottleSlipGamma = FMath::FInterpTo(ThrottleSlipGamma, throttle, AsyncTime, slipInterp);

			LateralForceScalar = FMath::FInterpTo(LateralForceScalar, DynamicLateralForce, AsyncTime, slipInterp);
			float d = LinearVelocity.X < 0.f && ThrottleSlipGamma > 0.5f ? -1.f : 1.f;
			WheelAngularVelocityMultiplier = FMath::FInterpTo(WheelAngularVelocityMultiplier, d * DynamicAngularVelocity, AsyncTime, slipInterp);

			if (!Hit.bBlockingHit)
			{
				float airGamma = FMath::Lerp(InAirSlowdownSpeed, InAirSpeedUp, throttle);
				float dynSpeed=  (throttle-brakes) * AirRotationMultiplier;
				BurnoutRotation = FMath::Clamp(FMath::FInterpTo(BurnoutRotation, dynSpeed, AsyncTime, airGamma),0.f,10000.f);
			}
			else
			{
				BurnoutRotation = 0.f;
			}

		}
		else
		{
			float dynSpeed = Hit.bBlockingHit ? 0.f : 1.f;

			BurnoutRotation = FMath::FInterpTo(BurnoutRotation, dynSpeed, AsyncTime, InAirSlowdownSpeed);
		}

	}

	PreviousCompression = compression;

	if (bApplyContactForceOnObjects)
	{
		if (Hit.bBlockingHit)
		{
			if (Hit.GetActor() != nullptr)
			{
				if (Hit.GetActor()->GetRootComponent()->IsSimulatingPhysics())
				{
					FVector surfaceForce = force * -100.f * Hit.Normal;
					Cast<UPrimitiveComponent>(Hit.GetActor()->GetRootComponent())->AddForceAtLocation(surfaceForce, Hit.ImpactPoint);
				}
			}
		}
	}

}

UStaticMeshComponent* USuspensionComponent::GetWheelMeshComponent()
{
	return WheelMeshComponent;
}


void USuspensionComponent::SetStiffness(float inStiffness)
{
	Stiffness = inStiffness;
}

void USuspensionComponent::SetDamping(float inDamping)
{
	Damping = inDamping;
}

void USuspensionComponent::SetExtensionLength(float inLength)
{
	ExtensionLength = inLength;
}


void USuspensionComponent::RenderWheelMesh()
{
	/*
#if WITH_EDITOR
	WheelMeshComponent->SetStaticMesh(WheelMesh);
	WheelMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, ExtensionLength * -1));

	if (bRotateWheel)
	{
		WheelMeshComponent->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	}
#endif
*/
}

bool USuspensionComponent::IsInAir()
{
	return !Hit.bBlockingHit;
}

float USuspensionComponent::GetCurrentLength()
{
	return CurrentLength;
}


FHitResult USuspensionComponent::GetHitResult()
{
	return Hit;
}

float USuspensionComponent::GetCompression()
{
	return OutCompression;
}


bool USuspensionComponent::IsHandbrakeWheel()
{
	return bHandbrakeWheel;
}


void USuspensionComponent::SetNewPhysicsBody(UPrimitiveComponent* physicsBody)
{
	if (physicsBody == nullptr)
	{
		return;
	}
	else
	{
		VehiclePrimitiveComponent = physicsBody;
	}

}

FVector USuspensionComponent::GetOffsetImpactPoint(float zOffset)
{
	return Hit.ImpactPoint + (this->GetForwardVector() * 20.f) + FVector(0.f,0.f,zOffset);
}

EHandbrakeWheelType USuspensionComponent::GetWheelHandbrake()
{
	return HandbrakeWheelType;
}

bool USuspensionComponent::IsLeftWheel()
{
	return bRotateWheel;
}

float USuspensionComponent::GetWheelRadius()
{
	return WheelRadius;
}

float USuspensionComponent::GetAngularVelocityMultiplier()
{
	return WheelAngularVelocityMultiplier;
}

float USuspensionComponent::GetBurnoutRotation()
{
	return BurnoutRotation;
}

float USuspensionComponent::GetSteeringRotation()
{
	return SteeringRotation;
}


float USuspensionComponent::GetWheelRelativeOffset()
{
	return WheelRelativeOffset;
}


FRotator USuspensionComponent::GetWheelLocalRotation()
{
	return WheelLocalRotation;
}

bool USuspensionComponent::GetIsDrivenWheel()
{
	return bDrivenWheel;
}

float USuspensionComponent::GetSlipRatio()
{
	return FrRatio;
}

float USuspensionComponent::GetSlipGamma()
{
	return ThrottleSlipGamma;
}

/*
float USuspensionComponent::GetWheelAnimLocation()
{
	return wheelAnimZ;
}

FRotator USuspensionComponent::GetAnimationRotator()
{
	return angularAnimRotator;
}

*/