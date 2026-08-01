// Copyright Impact-Forge. Game-agnostic concrete vehicle base, distilled from the BurningLands ABLVehicle.

#include "ForgeVehicle.h"

#include "ForgeVehicleExitPoint.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "Seats/ForgeSeatConfig.h"
#include "Components/ForgeVehicleLightComponent.h"
#include "Components/ForgeVehicleRunOverComponent.h"
#include "GAS/ForgeVehicleAttributeSet.h"
#include "ArcInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
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

	// The next three are optional rather than required. Everything deriving from this class is a
	// vehicle, but not everything is a crewed one: the drone archetypes suppress these with
	// DoNotCreateDefaultSubobject, and the engine only honours that against subobjects created as
	// optional - a required subobject is built anyway and the suppression is logged and ignored.
	// Consequently these three can be null on any subclass and every use must check.
	OccupantExitPoint = CreateOptionalDefaultSubobject<UForgeVehicleExitPoint>(TEXT("OccupantExitPoint"));
	if (OccupantExitPoint && Mesh)
	{
		OccupantExitPoint->SetupAttachment(Mesh);
	}

	IgnitionComponent = CreateDefaultSubobject<UForgeEngineIgnitionComponent>(TEXT("IgnitionComponent"));

	RunOverComponent = CreateOptionalDefaultSubobject<UForgeVehicleRunOverComponent>(TEXT("RunOverComponent"));

	VehicleInventory = CreateOptionalDefaultSubobject<UArcInventoryComponent>(TEXT("VehicleInventory"));

	// The vehicle owns its ability system rather than borrowing an occupant's, so vehicle state
	// outlives crew changes and works on unmanned platforms. Mixed replication: the possessing
	// client gets full fidelity, everyone else only what they need to predict/observe.
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetIsReplicated(true);
		AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	}

	VehicleAttributes = CreateDefaultSubobject<UForgeVehicleAttributeSet>(TEXT("VehicleAttributes"));
}

UAbilitySystemComponent* AForgeVehicle::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AForgeVehicle::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Owner and avatar are both the vehicle: it is its own gameplay actor.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (!HasAuthority())
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
	{
		if (!IsValid(Ability))
		{
			continue;
		}
		// Granted with the vehicle as SourceObject so UForgeVehicleAbility::GetOwningVehicle resolves
		// even when the ability instance is activated through an occupant's ability system.
		FGameplayAbilitySpec Spec(Ability, 1, INDEX_NONE, this);
		Spec.SourceObject = this;
		AbilitySystemComponent->GiveAbility(Spec);
	}
}

void AForgeVehicle::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystem();
}

void AForgeVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// Re-point actor info so the new controller becomes the ability system's owning connection.
	InitializeAbilitySystem();
}

void AForgeVehicle::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitializeAbilitySystem();
}

void AForgeVehicle::GetLightsOfType(const EForgeVehicleLightType LightType, TArray<UForgeVehicleLightComponent*>& OutLights) const
{
	OutLights.Reset();

	TInlineComponentArray<UForgeVehicleLightComponent*> Lights(this);
	for (UForgeVehicleLightComponent* Light : Lights)
	{
		if (IsValid(Light) && Light->LightType == LightType)
		{
			OutLights.Add(Light);
		}
	}
}

void AForgeVehicle::SetLightsOfType(const EForgeVehicleLightType LightType, const bool bOn)
{
	TArray<UForgeVehicleLightComponent*> Lights;
	GetLightsOfType(LightType, Lights);

	for (UForgeVehicleLightComponent* Light : Lights)
	{
		Light->SetLightOn(bOn);
	}
}

void AForgeVehicle::ToggleLightsOfType(const EForgeVehicleLightType LightType)
{
	TArray<UForgeVehicleLightComponent*> Lights;
	GetLightsOfType(LightType, Lights);

	if (Lights.Num() == 0)
	{
		return;
	}

	// Drive the whole group from the first fixture's state so a group can never end up split.
	const bool bNewState = !Lights[0]->IsLightOn();
	for (UForgeVehicleLightComponent* Light : Lights)
	{
		Light->SetLightOn(bNewState);
	}
}

bool AForgeVehicle::AreLightsOfTypeOn(const EForgeVehicleLightType LightType) const
{
	TArray<UForgeVehicleLightComponent*> Lights;
	GetLightsOfType(LightType, Lights);

	for (const UForgeVehicleLightComponent* Light : Lights)
	{
		if (Light->IsLightOn())
		{
			return true;
		}
	}
	return false;
}

bool AForgeVehicle::IsAvailableForInteraction_Implementation(const UPrimitiveComponent* InteractedComponent, const AActor* InteractingActor) const
{
	return true;
}

void AForgeVehicle::OnPostInteract_Implementation(const AActor* InteractingActor, const UPrimitiveComponent* InteractedComponent)
{
	// Let gameplay customise the interaction first (e.g. open a menu instead of entering).
	OnVehicleInteracted(const_cast<AActor*>(InteractingActor));

	// Default behaviour: seat the interacting actor's player in the first available seat.
	if (const APawn* InteractingPawn = Cast<APawn>(InteractingActor))
	{
		if (APlayerState* InteractingPlayerState = InteractingPawn->GetPlayerState())
		{
			RequestEnterAnySeat(InteractingPlayerState);
		}
	}
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

void AForgeVehicle::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Possession has already moved by the time this fires. If the contexts are still applied to a
	// controller that is no longer driving us, take them back. Without this, a raw UnPossess (one
	// that never runs the seat-exit path) leaves the vehicle's input contexts stuck on the player.
	APlayerController* PreviousPC = DriverInputController.Get();
	if (PreviousPC && PreviousPC != GetController())
	{
		UpdateDriverInputMapping(false, PreviousPC);
	}
}

void AForgeVehicle::UpdateDriverInputMapping(bool bAdd, APlayerController* ForController)
{
	if (!VehicleBaseMappingContext && !DriverMappingContext)
	{
		return;
	}

	APlayerController* PC = ForController ? ForController : Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	// Layered: the shared vehicle context sits underneath so a vehicle can override individual binds
	// without having to restate the common controls.
	if (bAdd)
	{
		if (VehicleBaseMappingContext)
		{
			Subsystem->AddMappingContext(VehicleBaseMappingContext, VehicleBaseMappingPriority);
		}
		if (DriverMappingContext)
		{
			Subsystem->AddMappingContext(DriverMappingContext, DriverMappingPriority);
		}
		DriverInputController = PC;
	}
	else
	{
		if (DriverMappingContext)
		{
			Subsystem->RemoveMappingContext(DriverMappingContext);
		}
		if (VehicleBaseMappingContext)
		{
			Subsystem->RemoveMappingContext(VehicleBaseMappingContext);
		}
		if (DriverInputController == PC)
		{
			DriverInputController.Reset();
		}
	}
}

void AForgeVehicle::UpdateSeatInputMapping(APlayerState* Player, UForgeVehicleSeatConfig* Seat, bool bAdd)
{
	FForgeSeatData SeatData;
	if (!IsValid(Player) || !GetSeatData(Seat, SeatData) || SeatData.InputMappingContext.IsNull())
	{
		return;
	}

	// Input contexts are per-local-player, so only the machine that owns this occupant does anything.
	const APlayerController* PC = Cast<APlayerController>(Player->GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	// Seat contexts are authored as soft refs; a synchronous load is acceptable here because seat
	// changes are discrete, player-driven events rather than per-frame work.
	if (UInputMappingContext* Context = SeatData.InputMappingContext.LoadSynchronous())
	{
		if (bAdd)
		{
			Subsystem->AddMappingContext(Context, SeatData.InputMappingPriority);
		}
		else
		{
			Subsystem->RemoveMappingContext(Context);
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

	// When the locally controlled occupant leaves the driver seat, drop the driver mapping contexts.
	if (SeatChangeEvent == EForgeVehicleSeatChangeType::ExitVehicle && FromSeat && FromSeat->IsDriverSeat())
	{
		UpdateDriverInputMapping(false);
	}

	// Per-seat contexts: swap the occupant's seat layer to match the seat they are now in. This is
	// what lets a gunner or specialist station carry different controls to the driver's.
	switch (SeatChangeEvent)
	{
	case EForgeVehicleSeatChangeType::EnterVehicle:
		UpdateSeatInputMapping(Player, ToSeat, true);
		break;
	case EForgeVehicleSeatChangeType::SwitchSeats:
		UpdateSeatInputMapping(Player, FromSeat, false);
		UpdateSeatInputMapping(Player, ToSeat, true);
		break;
	case EForgeVehicleSeatChangeType::ExitVehicle:
		UpdateSeatInputMapping(Player, FromSeat, false);
		break;
	default:
		break;
	}
}
