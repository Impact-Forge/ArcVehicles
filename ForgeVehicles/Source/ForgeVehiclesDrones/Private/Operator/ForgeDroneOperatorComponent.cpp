// Copyright Impact-Forge. Drone operator control implementation.

#include "Operator/ForgeDroneOperatorComponent.h"

#include "ForgeVehiclesDrones.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Systems/ForgeDroneLinkComponent.h"

UForgeDroneOperatorComponent::UForgeDroneOperatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UForgeDroneOperatorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeDroneOperatorComponent, OperatorPlayerState);
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

EForgeDroneControlResult UForgeDroneOperatorComponent::TakeControl(AController* NewOperator)
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

	// Already flying this one: succeed quietly rather than tearing down and rebuilding the same state.
	if (OperatorController.Get() == NewOperator)
	{
		return EForgeDroneControlResult::Granted;
	}

	if (IsControlled())
	{
		// Someone else has it. Refuse rather than stealing it out from under them - but a stale entry
		// left by a disconnect should not lock the drone out forever, so only a live controller counts.
		if (OperatorController.IsValid())
		{
			return EForgeDroneControlResult::AlreadyControlled;
		}
		ClearOperatorState();
	}

	APawn* Body = NewOperator->GetPawn();
	if (!IsValid(Body))
	{
		// No body means nothing to come back to. Refusing here is kinder than granting control and
		// stranding the player when they release it.
		return EForgeDroneControlResult::NoOperatorBody;
	}

	APawn* DronePawn = Cast<APawn>(Drone);
	if (!DronePawn)
	{
		return EForgeDroneControlResult::Unavailable;
	}

	OperatorController = NewOperator;
	OperatorBody = Body;
	OperatorPlayerState = NewOperator->PlayerState;

	// The body stays exactly where it is: standing in the open, visible, and shootable. Nothing is
	// attached, hidden or teleported, because that exposure is the price of flying the drone.
	// Keeping it owned by its controller means it carries on replicating to that client while parked,
	// so the operator can see their own body being shot at.
	NewOperator->Possess(DronePawn);
	if (IsValid(Body))
	{
		Body->SetOwner(NewOperator);
		Body->OnDestroyed.AddUniqueDynamic(this, &UForgeDroneOperatorComponent::HandleOperatorBodyDestroyed);
	}

	// The radio is on the soldier, not on the drone, so the link is measured from the body.
	if (UForgeDroneLinkComponent* Link = GetLink())
	{
		Link->SetOperator(NewOperator, Body);
	}

	OnControlTaken.Broadcast(NewOperator);
	UE_LOG(LogForgeDrones, Verbose, TEXT("%s taken by %s."), *GetNameSafe(Drone), *GetNameSafe(NewOperator));

	return EForgeDroneControlResult::Granted;
}

bool UForgeDroneOperatorComponent::ReleaseControl()
{
	AActor* Drone = GetOwner();
	if (!Drone || !Drone->HasAuthority() || !IsControlled())
	{
		return false;
	}

	AController* Operator = OperatorController.Get();
	APawn* Body = OperatorBody.Get();

	ClearOperatorState();

	// Clearing the operator drops link quality to zero, so an abandoned drone runs its failsafe rather
	// than hanging in the air on the last stick inputs it was given.
	if (UForgeDroneLinkComponent* Link = GetLink())
	{
		Link->ClearOperator();
	}

	if (IsValid(Operator))
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

	OnControlReleased.Broadcast(Operator);
	return true;
}

void UForgeDroneOperatorComponent::ServerRequestRelease_Implementation()
{
	ReleaseControl();
}

bool UForgeDroneOperatorComponent::IsLocalOperator() const
{
	// Controllers only exist on the server and on their own client, so a valid local one is proof.
	if (const AController* Operator = OperatorController.Get())
	{
		return Operator->IsLocalController();
	}

	// Remote clients have to go through the replicated player state instead.
	if (const APawn* DronePawn = Cast<APawn>(GetOwner()))
	{
		return OperatorPlayerState != nullptr && DronePawn->IsLocallyControlled();
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
		// ReleaseControl finds no body and unpossesses, which is the realistic outcome: the radio died
		// with the soldier holding it.
		ReleaseControl();
	}
}

UForgeDroneLinkComponent* UForgeDroneOperatorComponent::GetLink() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UForgeDroneLinkComponent>() : nullptr;
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

void UForgeDroneOperatorComponent::OnRep_OperatorPlayerState()
{
	// Clients learn who is flying the drone here. Nothing to drive yet - the hook exists so HUD and
	// nameplate code has a single place to react instead of polling.
}
