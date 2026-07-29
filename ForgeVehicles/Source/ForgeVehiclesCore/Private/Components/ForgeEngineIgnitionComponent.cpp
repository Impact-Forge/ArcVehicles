// Copyright Impact-Forge. Ignition state machine generalised from the BurningLands reference implementation.

#include "Components/ForgeEngineIgnitionComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

UForgeEngineIgnitionComponent::UForgeEngineIgnitionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);

	bWantsInitializeComponent = true;
	bAutoActivate = true;

	bEngineAlreadyStarted = false;
	EngineStartTime = 1.f;
	EngineStopTime = 1.f;
	EngineIgnitionState = EForgeEngineIgnitionState::Off;
	EngineIgnitionCooldown = 1.f;
}

void UForgeEngineIgnitionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	SharedParams.Condition = COND_SkipOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, EngineIgnitionState, SharedParams);
}

void UForgeEngineIgnitionComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (bEngineAlreadyStarted)
	{
		ChangeState(EForgeEngineIgnitionState::On);
	}
}

void UForgeEngineIgnitionComponent::RequestEngageIgnition()
{
	if (IsValid(GetOwner()) && GetOwner()->GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (CanEngageIgnition())
		{
			if (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
			{
				ServerInitiateIgnition(false);
			}

			EngageIgnition();
		}
	}
}

void UForgeEngineIgnitionComponent::RequestDisengageIgnition()
{
	if (IsValid(GetOwner()) && GetOwner()->GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (CanDisengageIgnition())
		{
			if (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
			{
				ServerInitiateIgnition(true);
			}

			DisengageIgnition();
		}
	}
}

void UForgeEngineIgnitionComponent::RequestReleaseIgnition()
{
	if (IsValid(GetOwner()) && GetOwner()->GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (CanReleaseIgnition())
		{
			if (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy)
			{
				ServerReleaseIgnition();
			}

			ReleaseIgnition();
		}
	}
}

float UForgeEngineIgnitionComponent::GetEngageIgnitionTimeRemaining() const
{
	const float TimeRemaining = GetWorld()->GetTimerManager().GetTimerRemaining(EngagingIgnitionHandle);
	return GetWorld()->GetTimerManager().IsTimerActive(EngagingIgnitionHandle) ? TimeRemaining : -1.f;
}

float UForgeEngineIgnitionComponent::GetDisengageIgnitionTimeRemaining() const
{
	const float TimeRemaining = GetWorld()->GetTimerManager().GetTimerRemaining(DisengagingIgnitionHandle);
	return GetWorld()->GetTimerManager().IsTimerActive(DisengagingIgnitionHandle) ? TimeRemaining : -1.f;
}

float UForgeEngineIgnitionComponent::GetIgnitionCooldownTimeRemaining() const
{
	const float TimeRemaining = GetWorld()->GetTimerManager().GetTimerRemaining(IgnitionCooldownHandle);
	return GetWorld()->GetTimerManager().IsTimerActive(IgnitionCooldownHandle) ? TimeRemaining : -1.f;
}

bool UForgeEngineIgnitionComponent::CanEngageIgnition() const
{
	return EngineIgnitionState == EForgeEngineIgnitionState::Off
		&& !GetWorld()->GetTimerManager().IsTimerActive(EngagingIgnitionHandle)
		&& !GetWorld()->GetTimerManager().IsTimerActive(IgnitionCooldownHandle)
		&& IsActive();
}

bool UForgeEngineIgnitionComponent::CanDisengageIgnition() const
{
	return EngineIgnitionState == EForgeEngineIgnitionState::On
		&& !GetWorld()->GetTimerManager().IsTimerActive(DisengagingIgnitionHandle)
		&& !GetWorld()->GetTimerManager().IsTimerActive(IgnitionCooldownHandle);
}

bool UForgeEngineIgnitionComponent::CanReleaseIgnition() const
{
	return EngineIgnitionState == EForgeEngineIgnitionState::Cutoff || EngineIgnitionState == EForgeEngineIgnitionState::Igniting;
}

void UForgeEngineIgnitionComponent::EngageIgnition()
{
	if (EngineStartTime > 0.f)
	{
		ChangeState(EForgeEngineIgnitionState::Igniting);
		GetWorld()->GetTimerManager().SetTimer(EngagingIgnitionHandle, this, &ThisClass::EngagingIgnitionElapsed, EngineStartTime);
	}
	else
	{
		ChangeState(EForgeEngineIgnitionState::On);
		EngagingIgnitionElapsed();
	}

	StartIgnitionCooldown();
}

void UForgeEngineIgnitionComponent::DisengageIgnition()
{
	if (EngineStopTime > 0.f)
	{
		ChangeState(EForgeEngineIgnitionState::Cutoff);
		GetWorld()->GetTimerManager().SetTimer(DisengagingIgnitionHandle, this, &ThisClass::DisengagingIgnitionElapsed, EngineStopTime);
	}
	else
	{
		ChangeState(EForgeEngineIgnitionState::Off);
		DisengagingIgnitionElapsed();
	}

	StartIgnitionCooldown();
}

void UForgeEngineIgnitionComponent::ReleaseIgnition()
{
	// Clear the Ignition Handles so that they dont elapse as we revert to the previous state.
	GetWorld()->GetTimerManager().ClearTimer(EngagingIgnitionHandle);
	GetWorld()->GetTimerManager().ClearTimer(DisengagingIgnitionHandle);

	// Releasing Ignition can only result in 2 states, On or Off, as being in a pre-release state implies the state it should revert to.
	// EForgeEngineIgnitionState::Cutoff being we were already On and should revert to On, EForgeEngineIgnitionState::Igniting implying we were Off and should revert to Off.
	// Revert back to the state we would have been in.
	const EForgeEngineIgnitionState NewState = EngineIgnitionState == EForgeEngineIgnitionState::Cutoff ? EForgeEngineIgnitionState::On : EForgeEngineIgnitionState::Off;
	ChangeState(NewState);
}

void UForgeEngineIgnitionComponent::ChangeState(EForgeEngineIgnitionState NewState)
{
	const EForgeEngineIgnitionState OldState = EngineIgnitionState;

	EngineIgnitionState = NewState;
	OnRep_EngineIgnitionState(OldState);

	if (IsValid(GetOwner()) && GetOwner()->HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, EngineIgnitionState, this);
		GetOwner()->ForceNetUpdate();
	}
}

void UForgeEngineIgnitionComponent::StartIgnitionCooldown()
{
	if (EngineIgnitionCooldown > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(IgnitionCooldownHandle, this, &ThisClass::IgnitionCooldownElapsed, EngineIgnitionCooldown);
	}
	else
	{
		IgnitionCooldownElapsed();
	}
}

void UForgeEngineIgnitionComponent::IgnitionCooldownElapsed()
{
	OnEngineIgnitionCooldownElapsed.Broadcast(this);
}

void UForgeEngineIgnitionComponent::EngagingIgnitionElapsed()
{
	ChangeState(EForgeEngineIgnitionState::On);
}

void UForgeEngineIgnitionComponent::DisengagingIgnitionElapsed()
{
	ChangeState(EForgeEngineIgnitionState::Off);
}

void UForgeEngineIgnitionComponent::ServerInitiateIgnition_Implementation(bool bDisengageIgnition /*= false*/)
{
	if (bDisengageIgnition)
	{
		DisengageIgnition();
	}
	else
	{
		EngageIgnition();
	}
}

bool UForgeEngineIgnitionComponent::ServerInitiateIgnition_Validate(bool bDisengageIgnition /*= false*/)
{
	return true;
}

void UForgeEngineIgnitionComponent::ServerReleaseIgnition_Implementation()
{
	ReleaseIgnition();
}

bool UForgeEngineIgnitionComponent::ServerReleaseIgnition_Validate()
{
	return true;
}

void UForgeEngineIgnitionComponent::OnRep_EngineIgnitionState(EForgeEngineIgnitionState PreviousState)
{
	// Detect and broadcast if we have Released Ignition.
	if ((PreviousState == EForgeEngineIgnitionState::Cutoff && EngineIgnitionState == EForgeEngineIgnitionState::On)
		|| (PreviousState == EForgeEngineIgnitionState::Igniting && EngineIgnitionState == EForgeEngineIgnitionState::Off))
	{
		OnEngineReleasedIgnition.Broadcast(this, EngineIgnitionState);
	}

	OnEngineIgnitionStateChanged.Broadcast(this, EngineIgnitionState, PreviousState);
}
