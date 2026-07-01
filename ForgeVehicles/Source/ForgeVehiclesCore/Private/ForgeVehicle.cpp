// Copyright Impact-Forge. Game-agnostic concrete vehicle base, distilled from the BurningLands ABLVehicle.

#include "ForgeVehicle.h"

#include "ForgeVehicleExitPoint.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "Seats/ForgeSeatConfig.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

AForgeVehicle::AForgeVehicle(const FObjectInitializer& ObjectInitializer)
	: Super()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);

	bIsThrottleCollective = false;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleMesh"));
	if (Mesh)
	{
		Mesh->SetCollisionProfileName(TEXT("Vehicle"));
		// Physics simulation is left to the concrete vehicle type / Blueprint body so this base does
		// not fight movement solutions (e.g. RTune) that build and drive their own root primitive.
		// Only claim the root if a subclass/Blueprint has not already established one.
		if (GetRootComponent() == nullptr)
		{
			SetRootComponent(Mesh);
		}
	}

	OccupantExitPoint = CreateDefaultSubobject<UForgeVehicleExitPoint>(TEXT("OccupantExitPoint"));
	if (OccupantExitPoint && Mesh)
	{
		OccupantExitPoint->SetupAttachment(Mesh);
	}

	IgnitionComponent = CreateDefaultSubobject<UForgeEngineIgnitionComponent>(TEXT("IgnitionComponent"));
}

void AForgeVehicle::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// If a concrete subclass established its own root (e.g. the K2 aircraft "Core" or a boat hull),
	// keep the inherited default Mesh tidy by parenting it under that root instead of leaving it loose.
	if (Mesh && GetRootComponent() && Mesh != GetRootComponent() && Mesh->GetAttachParent() == nullptr)
	{
		Mesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	if (IgnitionComponent)
	{
		IgnitionComponent->OnEngineIgnitionStateChanged.AddUniqueDynamic(this, &AForgeVehicle::OnEngineIgnitionStateChanged);
	}
}

void AForgeVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (ThrottleAction)
		{
			EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AForgeVehicle::Input_Throttle);
			EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AForgeVehicle::Input_Throttle);
		}
		if (SteeringAction)
		{
			EnhancedInput->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &AForgeVehicle::Input_Steering);
			EnhancedInput->BindAction(SteeringAction, ETriggerEvent::Completed, this, &AForgeVehicle::Input_Steering);
		}
		if (VerticalAction)
		{
			EnhancedInput->BindAction(VerticalAction, ETriggerEvent::Triggered, this, &AForgeVehicle::Input_Vertical);
			EnhancedInput->BindAction(VerticalAction, ETriggerEvent::Completed, this, &AForgeVehicle::Input_Vertical);
		}
		if (EngineToggleAction)
		{
			EnhancedInput->BindAction(EngineToggleAction, ETriggerEvent::Started, this, &AForgeVehicle::Input_ToggleEngine);
		}
	}
}

void AForgeVehicle::PawnClientRestart()
{
	Super::PawnClientRestart();
	UpdateDriverInputMapping(true);
}

void AForgeVehicle::UpdateDriverInputMapping(bool bAdd)
{
	if (!DriverMappingContext)
	{
		return;
	}

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (bAdd)
				{
					Subsystem->AddMappingContext(DriverMappingContext, DriverMappingPriority);
				}
				else
				{
					Subsystem->RemoveMappingContext(DriverMappingContext);
				}
			}
		}
	}
}

TScriptInterface<IForgeVehicleMovementInterface> AForgeVehicle::GetVehicleMovementInterface()
{
	TScriptInterface<IForgeVehicleMovementInterface> Result;
	if (IForgeVehicleMovementInterface* AsInterface = Cast<IForgeVehicleMovementInterface>(this))
	{
		Result.SetObject(this);
		Result.SetInterface(AsInterface);
	}
	return Result;
}

bool AForgeVehicle::GetSeatData(UForgeVehicleSeatConfig* Seat, FForgeSeatData& OutSeatData) const
{
	if (const UForgeSeatConfig* ForgeSeat = Cast<UForgeSeatConfig>(Seat))
	{
		OutSeatData = ForgeSeat->SeatData;
		return true;
	}
	return false;
}

void AForgeVehicle::ToggleEngine()
{
	if (!IgnitionComponent)
	{
		return;
	}

	switch (IgnitionComponent->GetEngineIgnitionState())
	{
	case EForgeEngineIgnitionState::Off:
		IgnitionComponent->RequestEngageIgnition();
		break;
	case EForgeEngineIgnitionState::On:
		IgnitionComponent->RequestDisengageIgnition();
		break;
	case EForgeEngineIgnitionState::Igniting:
	case EForgeEngineIgnitionState::Cutoff:
		// Mid-transition: allow cancelling the in-progress change.
		IgnitionComponent->RequestReleaseIgnition();
		break;
	default:
		break;
	}
}

void AForgeVehicle::OnEngineIgnitionStateChanged(UForgeEngineIgnitionComponent* Component, EForgeEngineIgnitionState NewState, EForgeEngineIgnitionState OldState)
{
	TScriptInterface<IForgeVehicleMovementInterface> Movement = GetVehicleMovementInterface();
	if (!Movement)
	{
		return;
	}

	if (NewState == EForgeEngineIgnitionState::On)
	{
		Movement->StartEngine();
	}
	else if (NewState == EForgeEngineIgnitionState::Off)
	{
		Movement->StopEngine();
	}
}

void AForgeVehicle::Input_Throttle(const FInputActionValue& Value)
{
	if (TScriptInterface<IForgeVehicleMovementInterface> Movement = GetVehicleMovementInterface())
	{
		Movement->SetThrottleInput(Value.Get<float>() * ThrottleInputCoefficient);
	}
}

void AForgeVehicle::Input_Steering(const FInputActionValue& Value)
{
	if (TScriptInterface<IForgeVehicleMovementInterface> Movement = GetVehicleMovementInterface())
	{
		Movement->SetSteeringInput(Value.Get<float>() * SteeringInputCoefficient);
	}
}

void AForgeVehicle::Input_Vertical(const FInputActionValue& Value)
{
	if (TScriptInterface<IForgeVehicleMovementInterface> Movement = GetVehicleMovementInterface())
	{
		Movement->SetVerticalInput(Value.Get<float>() * VerticalInputCoefficient);
	}
}

void AForgeVehicle::Input_ToggleEngine(const FInputActionValue& Value)
{
	ToggleEngine();
}

void AForgeVehicle::NotifyPlayerSeatChangeEvent_Implementation(APlayerState* Player, UForgeVehicleSeatConfig* ToSeat, UForgeVehicleSeatConfig* FromSeat, EForgeVehicleSeatChangeType SeatChangeEvent)
{
	Super::NotifyPlayerSeatChangeEvent_Implementation(Player, ToSeat, FromSeat, SeatChangeEvent);

	// When the locally controlled occupant leaves the driver seat, drop the driver mapping context.
	if (SeatChangeEvent == EForgeVehicleSeatChangeType::ExitVehicle && FromSeat && FromSeat->IsDriverSeat())
	{
		UpdateDriverInputMapping(false);
	}
}
