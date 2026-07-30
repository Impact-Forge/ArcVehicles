// Copyright Impact-Forge. Drone operator control implementation.

#include "Operator/ForgeDroneOperatorComponent.h"

#include "Flight/ForgeMultirotorVehicle.h"
#include "ForgeVehicle.h"
#include "ForgeVehiclesDrones.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "Net/UnrealNetwork.h"
#include "Payloads/ForgeDroneDropReleaseComponent.h"
#include "Payloads/ForgeDroneGimbalComponent.h"
#include "Payloads/ForgeDroneWarheadComponent.h"
#include "Systems/ForgeDroneAutopilotComponent.h"
#include "Systems/ForgeDroneLinkComponent.h"

namespace
{
	/* Axis indices, shared between the frame layout and the link's per-axis degradation. */
	enum class EForgeRelayAxis : int32
	{
		Longitudinal = 0,
		Lateral = 1,
		Yaw = 2,
		Vertical = 3,
		GimbalPitch = 4,
		GimbalYaw = 5
	};

	constexpr int32 AxisIndex(const EForgeRelayAxis Axis) { return static_cast<int32>(Axis); }
}

UForgeDroneOperatorComponent::UForgeDroneOperatorComponent()
{
	// Only ticks while a relay is running; see UpdateTickEnabled.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UForgeDroneOperatorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeDroneOperatorComponent, OperatorPlayerState);
	DOREPLIFETIME(UForgeDroneOperatorComponent, ControlMode);
}

void UForgeDroneOperatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A drone that is shot down, runs out of battery and crashes, or is simply removed must not take its
	// operator with it. Handing the controller back before the pawn goes away is the difference between
	// losing a drone and being left staring at nothing.
	//
	// Only for the drone actually going away. On level transition or shutdown the whole world is being
	// torn down and there is no body left to return anyone to.
	const bool bDroneIsLeavingTheWorld =
		EndPlayReason == EEndPlayReason::Destroyed || EndPlayReason == EEndPlayReason::RemovedFromWorld;

	if (bDroneIsLeavingTheWorld && GetOwner() && GetOwner()->HasAuthority() && IsControlled())
	{
		ReleaseControl();
	}

	Super::EndPlay(EndPlayReason);
}

//~ Shared bookkeeping ------------------------------------------------------------------------------

EForgeDroneControlResult UForgeDroneOperatorComponent::BeginControl(AController* NewOperator, const EForgeDroneControlMode Mode)
{
	AActor* Drone = GetOwner();
	if (!Drone || !Drone->HasAuthority() || Drone->IsActorBeingDestroyed())
	{
		return EForgeDroneControlResult::Unavailable;
	}

	if (!IsValid(NewOperator))
	{
		return EForgeDroneControlResult::InvalidOperator;
	}

	// Already flying this one the same way: succeed quietly rather than tearing down and rebuilding
	// identical state.
	if (OperatorController.Get() == NewOperator && ControlMode == Mode)
	{
		return EForgeDroneControlResult::Granted;
	}

	if (IsControlled())
	{
		// Someone else has it. Refuse rather than stealing it out from under them - but a stale entry
		// left by a disconnect should not lock the drone out forever, so only a live controller counts.
		if (OperatorController.IsValid() && OperatorController.Get() != NewOperator)
		{
			return EForgeDroneControlResult::AlreadyControlled;
		}

		// Same operator switching routes, or a dead entry being cleaned up: release first so the old
		// route is unwound properly.
		ReleaseControl();
	}

	APawn* Body = NewOperator->GetPawn();
	if (!IsValid(Body))
	{
		// No body means no antenna and, for the possession route, nothing to come back to. Refusing here
		// is kinder than granting control and stranding the player when they release it.
		return EForgeDroneControlResult::NoOperatorBody;
	}

	OperatorController = NewOperator;
	OperatorBody = Body;
	OperatorPlayerState = NewOperator->PlayerState;
	Body->OnDestroyed.AddUniqueDynamic(this, &UForgeDroneOperatorComponent::HandleOperatorBodyDestroyed);

	// The radio is on the soldier, not on the drone, so the link is measured from the body.
	if (UForgeDroneLinkComponent* Link = GetLink())
	{
		Link->SetOperator(NewOperator, Body);
	}

	return EForgeDroneControlResult::Granted;
}

//~ Phase A: possession -----------------------------------------------------------------------------

EForgeDroneControlResult UForgeDroneOperatorComponent::TakeControl(AController* NewOperator)
{
	APawn* DronePawn = Cast<APawn>(GetOwner());
	if (!DronePawn)
	{
		return EForgeDroneControlResult::Unavailable;
	}

	const EForgeDroneControlResult Result = BeginControl(NewOperator, EForgeDroneControlMode::Possessed);
	if (Result != EForgeDroneControlResult::Granted)
	{
		return Result;
	}

	// Nothing to do if this is a no-op re-request.
	if (ControlMode == EForgeDroneControlMode::Possessed)
	{
		return Result;
	}

	ControlMode = EForgeDroneControlMode::Possessed;

	// The body stays exactly where it is: standing in the open, visible, and shootable. Nothing is
	// attached, hidden or teleported, because that exposure is the price of flying the drone.
	// Keeping it owned by its controller means it carries on replicating to that client while parked,
	// so the operator can see their own body being shot at.
	APawn* Body = OperatorBody.Get();
	NewOperator->Possess(DronePawn);
	if (IsValid(Body))
	{
		Body->SetOwner(NewOperator);
	}

	OnControlModeChanged.Broadcast(NewOperator);
	OnControlTaken.Broadcast(NewOperator);
	UE_LOG(LogForgeDrones, Verbose, TEXT("%s possessed by %s."), *GetNameSafe(GetOwner()), *GetNameSafe(NewOperator));

	return EForgeDroneControlResult::Granted;
}

//~ Phase B: relay ----------------------------------------------------------------------------------

EForgeDroneControlResult UForgeDroneOperatorComponent::BeginRelayControl(AController* NewOperator)
{
	AActor* Drone = GetOwner();

	const EForgeDroneControlResult Result = BeginControl(NewOperator, EForgeDroneControlMode::Relayed);
	if (Result != EForgeDroneControlResult::Granted)
	{
		return Result;
	}

	if (ControlMode == EForgeDroneControlMode::Relayed)
	{
		return Result;
	}

	ControlMode = EForgeDroneControlMode::Relayed;
	bHasAppliedFrame = false;
	TimeSinceFrameReceived = 0.f;

	// Nothing is possessed or unpossessed here, which is the whole point of this route: the operator's
	// body stays theirs, under their own control, and the possession and input-teardown paths are never
	// touched.
	//
	// The drone does need to belong to the operator's connection, though, or their input frames have no
	// route to the server and the aircraft is never relevant to them.
	PreRelayOwner = Drone->GetOwner();
	Drone->SetOwner(NewOperator);

	// Apply the frame before the airframe integrates it. Without this the component and the actor tick
	// in whatever order the engine happens to pick, and half the time the aircraft flies a frame behind
	// the sticks - which is exactly the sort of latency the link is supposed to be the only source of.
	Drone->AddTickPrerequisiteComponent(this);

	if (APlayerController* PC = GetOperatorPlayerController())
	{
		PC->SetViewTargetWithBlend(Drone, RelayViewBlendSeconds);
	}

	UpdateTickEnabled();

	OnControlModeChanged.Broadcast(NewOperator);
	OnControlTaken.Broadcast(NewOperator);
	UE_LOG(LogForgeDrones, Verbose, TEXT("%s relayed to %s."), *GetNameSafe(Drone), *GetNameSafe(NewOperator));

	return EForgeDroneControlResult::Granted;
}

void UForgeDroneOperatorComponent::SetRelayFlightInput(const float Longitudinal, const float Lateral, const float InYaw, const float Vertical)
{
	PendingFrame.Longitudinal = FForgeDroneInputFrame::Quantise(Longitudinal);
	PendingFrame.Lateral = FForgeDroneInputFrame::Quantise(Lateral);
	PendingFrame.Yaw = FForgeDroneInputFrame::Quantise(InYaw);
	PendingFrame.Vertical = FForgeDroneInputFrame::Quantise(Vertical);
}

void UForgeDroneOperatorComponent::SetRelayGimbalInput(const float GimbalPitch, const float GimbalYaw)
{
	PendingFrame.GimbalPitch = FForgeDroneInputFrame::Quantise(GimbalPitch);
	PendingFrame.GimbalYaw = FForgeDroneInputFrame::Quantise(GimbalYaw);
}

void UForgeDroneOperatorComponent::SendRelayCommand(const EForgeDroneRelayCommand Command)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ExecuteRelayCommand(Command);
		return;
	}

	ServerSendRelayCommand(Command);
}

void UForgeDroneOperatorComponent::ServerSendRelayCommand_Implementation(EForgeDroneRelayCommand Command)
{
	// Only from the player actually flying it. Ownership already restricts who can reach this RPC, but a
	// released drone can briefly still be connection-owned.
	if (OperatorController.IsValid())
	{
		ExecuteRelayCommand(Command);
	}
}

void UForgeDroneOperatorComponent::ServerSendInputFrame_Implementation(FForgeDroneInputFrame Frame)
{
	if (!IsRelaying() || !OperatorController.IsValid())
	{
		return;
	}

	// Unreliable delivery reorders. Applying an older frame after a newer one would drag the aircraft
	// back through inputs the pilot has already moved on from, which reads as a stutter in the controls.
	if (bHasAppliedFrame && !Frame.IsNewerThan(LastAppliedSequence))
	{
		return;
	}

	LastAppliedFrame = Frame;
	LastAppliedSequence = Frame.Sequence;
	bHasAppliedFrame = true;
	TimeSinceFrameReceived = 0.f;
}

void UForgeDroneOperatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsRelaying())
	{
		return;
	}

	AActor* Drone = GetOwner();
	if (!Drone)
	{
		return;
	}

	// Operator's client: flush the accumulated sticks at a fixed rate, so how often the project pushes
	// input does not change what a drone costs on the wire.
	if (!Drone->HasAuthority() && IsLocalOperator())
	{
		TimeSinceFrameSent += DeltaTime;
		const float SendInterval = 1.f / FMath::Max(RelaySendHz, 1.f);
		if (TimeSinceFrameSent >= SendInterval)
		{
			TimeSinceFrameSent = 0.f;
			++PendingFrame.Sequence;
			ServerSendInputFrame(PendingFrame);
		}
	}

	if (Drone->HasAuthority())
	{
		// Listen server: the host's own sticks never travel over the wire, so take them directly rather
		// than waiting for a frame that will never arrive.
		if (IsLocalOperator())
		{
			LastAppliedFrame = PendingFrame;
			bHasAppliedFrame = true;
			TimeSinceFrameReceived = 0.f;
		}
		else
		{
			TimeSinceFrameReceived += DeltaTime;

			// A stalled stream must not leave the aircraft flying its last input forever. Centring the
			// sticks hands it to the link's failsafe, which is the same place a dead radio leads.
			if (bHasAppliedFrame && TimeSinceFrameReceived > RelayInputTimeoutSeconds)
			{
				LastAppliedFrame = FForgeDroneInputFrame();
			}
		}

		ApplyRelayInput(DeltaTime);
	}
}

void UForgeDroneOperatorComponent::ApplyRelayInput(const float DeltaTime)
{
	AForgeVehicle* Drone = Cast<AForgeVehicle>(GetOwner());
	if (!Drone)
	{
		return;
	}

	// An active autopilot owns the controls. Applying relayed sticks on top would have the pilot fighting
	// a return-to-home they did not ask to cancel.
	if (const UForgeDroneAutopilotComponent* Autopilot = Drone->FindComponentByClass<UForgeDroneAutopilotComponent>())
	{
		if (Autopilot->IsActive())
		{
			return;
		}
	}

	UForgeDroneLinkComponent* Link = GetLink();

	// Every command goes through the link, so a marginal signal is felt as sticky, delayed controls
	// rather than as an aircraft that has quietly become less capable.
	const auto Filtered = [Link](const EForgeRelayAxis Axis, const int8 Raw) -> float
	{
		const float Value = FForgeDroneInputFrame::Dequantise(Raw);
		return Link ? Link->FilterInput(AxisIndex(Axis), Value) : Value;
	};

	if (TScriptInterface<IForgeVehicleMovementInterface> Movement = Drone->GetVehicleMovementInterface())
	{
		Movement->SetThrottleInput(Filtered(EForgeRelayAxis::Longitudinal, LastAppliedFrame.Longitudinal));
		Movement->SetRollAxisInput(Filtered(EForgeRelayAxis::Lateral, LastAppliedFrame.Lateral));
		Movement->SetYawAxisInput(Filtered(EForgeRelayAxis::Yaw, LastAppliedFrame.Yaw));
		Movement->SetVerticalInput(Filtered(EForgeRelayAxis::Vertical, LastAppliedFrame.Vertical));
	}

	if (UForgeDroneGimbalComponent* Gimbal = Drone->FindComponentByClass<UForgeDroneGimbalComponent>())
	{
		// X yaws, Y pitches.
		const FVector2D LookInput(
			Filtered(EForgeRelayAxis::GimbalYaw, LastAppliedFrame.GimbalYaw),
			Filtered(EForgeRelayAxis::GimbalPitch, LastAppliedFrame.GimbalPitch));

		if (!LookInput.IsNearlyZero())
		{
			Gimbal->AddAimInput(LookInput, DeltaTime);
		}
	}
}

void UForgeDroneOperatorComponent::ExecuteRelayCommand(const EForgeDroneRelayCommand Command)
{
	AActor* Drone = GetOwner();
	if (!Drone || !Drone->HasAuthority())
	{
		return;
	}

	switch (Command)
	{
	case EForgeDroneRelayCommand::ArmMotors:
		if (AForgeVehicle* Vehicle = Cast<AForgeVehicle>(Drone))
		{
			if (TScriptInterface<IForgeVehicleMovementInterface> Movement = Vehicle->GetVehicleMovementInterface())
			{
				Movement->StartEngine();
			}
		}
		break;

	case EForgeDroneRelayCommand::DisarmMotors:
		if (AForgeVehicle* Vehicle = Cast<AForgeVehicle>(Drone))
		{
			if (TScriptInterface<IForgeVehicleMovementInterface> Movement = Vehicle->GetVehicleMovementInterface())
			{
				Movement->StopEngine();
			}
		}
		break;

	case EForgeDroneRelayCommand::ToggleFlightMode:
		if (AForgeMultirotorVehicle* Multirotor = Cast<AForgeMultirotorVehicle>(Drone))
		{
			Multirotor->SetFlightMode(Multirotor->GetFlightMode() == EForgeMultirotorFlightMode::Angle
				? EForgeMultirotorFlightMode::Acro
				: EForgeMultirotorFlightMode::Angle);
		}
		break;

	case EForgeDroneRelayCommand::ReleaseStore:
		if (UForgeDroneDropReleaseComponent* Stores = Drone->FindComponentByClass<UForgeDroneDropReleaseComponent>())
		{
			Stores->ReleaseStore();
		}
		break;

	case EForgeDroneRelayCommand::ArmWarhead:
		if (UForgeDroneWarheadComponent* Warhead = Drone->FindComponentByClass<UForgeDroneWarheadComponent>())
		{
			Warhead->RequestArm();
		}
		break;

	case EForgeDroneRelayCommand::ReturnToHome:
		if (UForgeDroneAutopilotComponent* Autopilot = Drone->FindComponentByClass<UForgeDroneAutopilotComponent>())
		{
			Autopilot->SetMode(EForgeDroneAutopilotMode::ReturnToHome);
		}
		break;

	case EForgeDroneRelayCommand::HoldPosition:
		if (UForgeDroneAutopilotComponent* Autopilot = Drone->FindComponentByClass<UForgeDroneAutopilotComponent>())
		{
			Autopilot->SetMode(EForgeDroneAutopilotMode::PositionHold);
		}
		break;

	case EForgeDroneRelayCommand::ResumeManual:
		if (UForgeDroneAutopilotComponent* Autopilot = Drone->FindComponentByClass<UForgeDroneAutopilotComponent>())
		{
			Autopilot->SetMode(EForgeDroneAutopilotMode::Manual);
		}
		break;

	default:
		break;
	}
}

//~ Release -----------------------------------------------------------------------------------------

bool UForgeDroneOperatorComponent::ReleaseControl()
{
	AActor* Drone = GetOwner();
	if (!Drone || !Drone->HasAuthority() || !IsControlled())
	{
		return false;
	}

	AController* Operator = OperatorController.Get();
	APawn* Body = OperatorBody.Get();
	const EForgeDroneControlMode ReleasedMode = ControlMode;
	AActor* RestoredOwner = PreRelayOwner.Get();

	ControlMode = EForgeDroneControlMode::None;
	ClearOperatorState();
	UpdateTickEnabled();

	// Clearing the operator drops link quality to zero, so an abandoned drone runs its failsafe rather
	// than hanging in the air on the last stick inputs it was given.
	if (UForgeDroneLinkComponent* Link = GetLink())
	{
		Link->ClearOperator();
	}

	if (ReleasedMode == EForgeDroneControlMode::Relayed)
	{
		// Nothing was possessed, so there is nothing to hand back - only the camera and the ownership
		// that let the input frames through.
		if (APlayerController* PC = Cast<APlayerController>(Operator))
		{
			// Back to whatever they are actually standing in. If their body has gone, the controller
			// itself is the fallback, which is where UE puts a pawnless player anyway.
			AActor* ViewTarget = IsValid(Body) ? Cast<AActor>(Body) : Cast<AActor>(PC->GetPawn());
			PC->SetViewTargetWithBlend(ViewTarget ? ViewTarget : Cast<AActor>(PC), RelayViewBlendSeconds);
		}
		Drone->SetOwner(IsValid(RestoredOwner) ? RestoredOwner : nullptr);
		Drone->RemoveTickPrerequisiteComponent(this);
	}
	else if (IsValid(Operator))
	{
		if (IsValid(Body))
		{
			Operator->Possess(Body);
		}
		else
		{
			// The body died mid-flight. Let go of the drone anyway - otherwise the aircraft stays under
			// the control of a player who no longer exists on the ground - and leave the controller
			// pawnless, which is the same state any other death produces and which the project's death
			// handling already deals with.
			if (Operator->GetPawn() == Drone)
			{
				Operator->UnPossess();
			}
			UE_LOG(LogForgeDrones, Verbose, TEXT("%s released by %s, whose body was already gone."),
				*GetNameSafe(Drone), *GetNameSafe(Operator));
		}
	}

	// Centre the sticks so a re-taken drone does not inherit the last pilot's inputs.
	LastAppliedFrame = FForgeDroneInputFrame();
	PendingFrame = FForgeDroneInputFrame();
	bHasAppliedFrame = false;
	PreRelayOwner = nullptr;

	OnControlModeChanged.Broadcast(Operator);
	OnControlReleased.Broadcast(Operator);
	return true;
}

void UForgeDroneOperatorComponent::ServerRequestRelease_Implementation()
{
	ReleaseControl();
}

//~ State -------------------------------------------------------------------------------------------

bool UForgeDroneOperatorComponent::IsLocalOperator() const
{
	// Controllers only exist on the server and on their own client, so a valid local one is proof.
	if (const AController* Operator = OperatorController.Get())
	{
		return Operator->IsLocalController();
	}

	if (!OperatorPlayerState)
	{
		return false;
	}

	// A relayed drone is not possessed, so the client has to recognise itself through the replicated
	// player state instead.
	if (const APlayerController* LocalPC = OperatorPlayerState->GetPlayerController())
	{
		return LocalPC->IsLocalController();
	}

	// Possession route: the drone is the client's own pawn.
	if (const APawn* DronePawn = Cast<APawn>(GetOwner()))
	{
		return DronePawn->IsLocallyControlled();
	}

	return false;
}

void UForgeDroneOperatorComponent::HandleOperatorBodyDestroyed(AActor* DestroyedActor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	AController* Operator = OperatorController.Get();

	// The antenna is gone with the body, so the link is dead regardless of what happens to the player.
	// The drone falls back on its failsafe from here.
	if (UForgeDroneLinkComponent* Link = GetLink())
	{
		Link->ClearOperator();
	}

	OperatorBody = nullptr;

	UE_LOG(LogForgeDrones, Verbose, TEXT("%s lost its operator's body."), *GetNameSafe(GetOwner()));
	OnOperatorBodyLost.Broadcast(Operator);

	if (bReleaseControlOnOperatorBodyLost)
	{
		// ReleaseControl finds no body and unpossesses (or drops the relay), which is the realistic
		// outcome: the radio died with the soldier holding it.
		ReleaseControl();
	}
}

//~ Internals ---------------------------------------------------------------------------------------

UForgeDroneLinkComponent* UForgeDroneOperatorComponent::GetLink() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UForgeDroneLinkComponent>() : nullptr;
}

APlayerController* UForgeDroneOperatorComponent::GetOperatorPlayerController() const
{
	return Cast<APlayerController>(OperatorController.Get());
}

void UForgeDroneOperatorComponent::ClearOperatorState()
{
	if (APawn* Body = OperatorBody.Get())
	{
		Body->OnDestroyed.RemoveDynamic(this, &UForgeDroneOperatorComponent::HandleOperatorBodyDestroyed);
	}

	OperatorController = nullptr;
	OperatorBody = nullptr;
	OperatorPlayerState = nullptr;
}

void UForgeDroneOperatorComponent::UpdateTickEnabled()
{
	// Only a relay needs a tick, and only on the two machines that do the work: the operator's client
	// packs frames, the server applies them. A possessed drone routes input the ordinary way.
	const AActor* Drone = GetOwner();
	const bool bNeedsTick = IsRelaying() && Drone && (Drone->HasAuthority() || IsLocalOperator());
	SetComponentTickEnabled(bNeedsTick);
}

void UForgeDroneOperatorComponent::OnRep_ControlMode()
{
	// Clients learn here that the drone has been claimed or released; the operator's own client also
	// needs its send loop started or stopped.
	UpdateTickEnabled();
	OnControlModeChanged.Broadcast(OperatorController.Get());
}

void UForgeDroneOperatorComponent::OnRep_OperatorPlayerState()
{
	// The mode and the operator arrive as separate properties with no guaranteed order, and a client
	// cannot tell whether it is the operator until it has the player state. Re-evaluating on both means
	// whichever lands second starts the send loop, instead of it depending on packet ordering.
	UpdateTickEnabled();
}
