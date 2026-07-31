// Copyright Impact-Forge. Gimbal implementation.

#include "Payloads/ForgeDroneGimbalComponent.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/* Aim updates are sent a few times a second: observers only need roughly where a camera points. */
	constexpr float AimReplicationInterval = 0.2f;
}

UForgeDroneGimbalComponent::UForgeDroneGimbalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UForgeDroneGimbalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeDroneGimbalComponent, ReplicatedAim);
}

void UForgeDroneGimbalComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveComponents();
	RecentreAim();
	CurrentWorldAim = DesiredWorldAim;
}

void UForgeDroneGimbalComponent::ResolveComponents()
{
	YawComponent = nullptr;
	PitchComponent = nullptr;

	if (!GetOwner())
	{
		return;
	}

	TInlineComponentArray<USceneComponent*> SceneComponents(GetOwner());
	for (USceneComponent* Component : SceneComponents)
	{
		if (!IsValid(Component))
		{
			continue;
		}
		if (!YawComponentName.IsNone() && Component->GetFName() == YawComponentName)
		{
			YawComponent = Component;
		}
		else if (!PitchComponentName.IsNone() && Component->GetFName() == PitchComponentName)
		{
			PitchComponent = Component;
		}
	}
}

void UForgeDroneGimbalComponent::RecentreAim()
{
	if (GetOwner())
	{
		DesiredWorldAim = FRotator(0.f, GetOwner()->GetActorRotation().Yaw, 0.f);
	}
}

void UForgeDroneGimbalComponent::AddAimInput(const FVector2D& LookInput, const float DeltaTime)
{
	if (LookInput.IsNearlyZero())
	{
		return;
	}

	DesiredWorldAim.Yaw += LookInput.X * LookSensitivityDeg * DeltaTime;
	DesiredWorldAim.Pitch = FMath::Clamp(
		DesiredWorldAim.Pitch + LookInput.Y * LookSensitivityDeg * DeltaTime,
		MinPitchDeg,
		MaxPitchDeg);
	DesiredWorldAim.Yaw = FRotator::NormalizeAxis(DesiredWorldAim.Yaw);
	DesiredWorldAim.Roll = 0.f;
}

void UForgeDroneGimbalComponent::LookAt(const FVector& WorldLocation)
{
	if (!PitchComponent && !YawComponent)
	{
		return;
	}

	const FVector Origin = PitchComponent ? PitchComponent->GetComponentLocation()
		: (YawComponent ? YawComponent->GetComponentLocation() : GetOwner()->GetActorLocation());

	FRotator Look = (WorldLocation - Origin).Rotation();
	Look.Pitch = FMath::Clamp(Look.Pitch, MinPitchDeg, MaxPitchDeg);
	Look.Roll = 0.f;
	DesiredWorldAim = Look;
}

void UForgeDroneGimbalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.f)
	{
		return;
	}

	// Observers follow the replicated aim; the authority and the controlling client run the real one.
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const bool bDrivesAim = !GetOwner()
		|| GetOwner()->HasAuthority()
		|| (OwnerPawn && OwnerPawn->IsLocallyControlled());
	if (!bDrivesAim)
	{
		DesiredWorldAim = ReplicatedAim;
	}

	// Slew toward the commanded aim rather than snapping: a real gimbal has finite motor speed, and
	// the lag is part of what makes drone footage read as drone footage.
	const float MaxStep = SlewRateDegPerSecond * DeltaTime;
	const FRotator Delta = (DesiredWorldAim - CurrentWorldAim).GetNormalized();
	CurrentWorldAim.Yaw = FRotator::NormalizeAxis(CurrentWorldAim.Yaw + FMath::Clamp(Delta.Yaw, -MaxStep, MaxStep));
	CurrentWorldAim.Pitch = FMath::Clamp(CurrentWorldAim.Pitch + FMath::Clamp(Delta.Pitch, -MaxStep, MaxStep), MinPitchDeg, MaxPitchDeg);
	CurrentWorldAim.Roll = 0.f;

	ApplyAimToComponents();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TimeSinceAimReplication += DeltaTime;
		if (TimeSinceAimReplication >= AimReplicationInterval)
		{
			TimeSinceAimReplication = 0.f;
			ReplicatedAim = CurrentWorldAim;
		}
	}

	OnAimChanged.Broadcast(CurrentWorldAim);
}

void UForgeDroneGimbalComponent::ApplyAimToComponents()
{
	if (!GetOwner())
	{
		return;
	}

	// The aim is held in world space and converted back into the airframe's frame every tick. That
	// conversion *is* the stabilisation: as the drone rolls and yaws, the required local rotation
	// changes to keep the camera pointing at the same place in the world.
	const FRotator ActorRotation = GetOwner()->GetActorRotation();

	if (YawComponent)
	{
		const float LocalYaw = FRotator::NormalizeAxis(CurrentWorldAim.Yaw - ActorRotation.Yaw);
		const float ClampedYaw = (YawLimitDeg >= 180.f) ? LocalYaw : FMath::Clamp(LocalYaw, -YawLimitDeg, YawLimitDeg);
		YawComponent->SetRelativeRotation(FRotator(0.f, ClampedYaw, 0.f));
	}

	if (PitchComponent)
	{
		// Pitch is taken relative to the airframe too, so a nose-down attitude does not tilt the view.
		const float LocalPitch = FMath::Clamp(CurrentWorldAim.Pitch - ActorRotation.Pitch, MinPitchDeg, MaxPitchDeg);
		// Counter-roll keeps the horizon level, which is the most visible part of stabilisation.
		PitchComponent->SetRelativeRotation(FRotator(LocalPitch, 0.f, -ActorRotation.Roll));
	}
}

void UForgeDroneGimbalComponent::OnRep_ReplicatedAim()
{
	// Observers adopt the replicated aim; slewing in Tick keeps the motion smooth between updates.
	DesiredWorldAim = ReplicatedAim;
}
