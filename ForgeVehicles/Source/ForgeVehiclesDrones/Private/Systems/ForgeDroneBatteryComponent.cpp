// Copyright Impact-Forge. Drone battery implementation.

#include "Systems/ForgeDroneBatteryComponent.h"

#include "Flight/ForgeMultirotorVehicle.h"
#include "ForgeFixedWingVehicle.h"
#include "GameFramework/Actor.h"
#include "Math/ForgeDroneMath.h"
#include "Net/UnrealNetwork.h"

UForgeDroneBatteryComponent::UForgeDroneBatteryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Charge integration does not need frame accuracy; a few hertz is plenty and keeps a swarm cheap.
	PrimaryComponentTick.TickInterval = 0.25f;
	SetIsReplicatedByDefault(true);
}

void UForgeDroneBatteryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeDroneBatteryComponent, ChargeFraction);
}

void UForgeDroneBatteryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ChargeFraction = FMath::Clamp(InitialChargeFraction, 0.f, 1.f);
	}

	// Only the server integrates charge; clients just receive it.
	SetComponentTickEnabled(GetOwner() && GetOwner()->HasAuthority());
}

void UForgeDroneBatteryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	CurrentLoadW = AvionicsLoadW + PayloadLoadW + GatherPropulsionLoadW();

	const float PreviousCharge = ChargeFraction;
	const float RemainingWh = ForgeDrone::Battery::IntegrateChargeWh(GetRemainingWh(), CurrentLoadW, DeltaTime);
	ChargeFraction = CapacityWh > 0.f ? FMath::Clamp(RemainingWh / CapacityWh, 0.f, 1.f) : 0.f;

	if (!FMath::IsNearlyEqual(PreviousCharge, ChargeFraction))
	{
		OnChargeChanged.Broadcast(ChargeFraction);
	}

	// Thresholds latch, so a pack hovering on the boundary does not spam warnings.
	if (!bDepletedReported && ChargeFraction <= 0.f)
	{
		bDepletedReported = true;
		ApplyPowerScaleToOwner(0.f);
		OnDepleted.Broadcast();
	}
	else if (!bCriticalReported && ChargeFraction <= CriticalChargeThreshold)
	{
		bCriticalReported = true;
		OnCriticalBattery.Broadcast();
	}
	else if (!bLowReported && ChargeFraction <= LowChargeThreshold)
	{
		bLowReported = true;
		OnLowBattery.Broadcast();
	}
}

float UForgeDroneBatteryComponent::GatherPropulsionLoadW() const
{
	if (const AForgeMultirotorVehicle* Multirotor = Cast<AForgeMultirotorVehicle>(GetOwner()))
	{
		// A rotor holding the aircraft up: shaft power climbs faster than thrust, which is why a quad
		// flown hard empties its pack in a fraction of its hover endurance.
		return ForgeDrone::Battery::PropulsionLoadW(Multirotor->GetMeanMotorOutput(), MaxPropulsionLoadW);
	}

	if (AForgeFixedWingVehicle* FixedWing = Cast<AForgeFixedWingVehicle>(GetOwner()))
	{
		// A wing carries its own weight, so the propeller only fights drag and the cruise curve applies.
		// A shut-down engine draws nothing - and the aircraft keeps flying, because it glides.
		if (!FixedWing->IsEngineRunning())
		{
			return 0.f;
		}
		return ForgeDrone::Battery::CruisePropulsionLoadW(FixedWing->GetThrottlePercent() * 0.01f, MaxPropulsionLoadW);
	}

	// Unknown airframes report their draw by driving SetPayloadLoadW externally; assume idle rather
	// than inventing a load.
	return 0.f;
}

void UForgeDroneBatteryComponent::ApplyPowerScaleToOwner(const float PowerScale) const
{
	if (AForgeMultirotorVehicle* Multirotor = Cast<AForgeMultirotorVehicle>(GetOwner()))
	{
		Multirotor->SetPowerScale(PowerScale);
		return;
	}

	if (AForgeFixedWingVehicle* FixedWing = Cast<AForgeFixedWingVehicle>(GetOwner()))
	{
		// There is no partial-power notion on the aero engine, so a flat pack simply shuts it down.
		// The wing keeps flying: it becomes a glider, which is what actually happens.
		if (PowerScale <= 0.f)
		{
			FixedWing->StopEngine();
		}
	}
}

float UForgeDroneBatteryComponent::GetEstimatedEnduranceMinutes() const
{
	return ForgeDrone::Battery::EnduranceMinutes(GetRemainingWh(), CurrentLoadW);
}

void UForgeDroneBatteryComponent::SetPayloadLoadW(const float NewPayloadLoadW)
{
	PayloadLoadW = FMath::Max(NewPayloadLoadW, 0.f);
}

void UForgeDroneBatteryComponent::Recharge(const float NewChargeFraction)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	ChargeFraction = FMath::Clamp(NewChargeFraction, 0.f, 1.f);
	bLowReported = false;
	bCriticalReported = false;
	bDepletedReported = false;

	ApplyPowerScaleToOwner(1.f);
	OnChargeChanged.Broadcast(ChargeFraction);
}

void UForgeDroneBatteryComponent::OnRep_ChargeFraction()
{
	OnChargeChanged.Broadcast(ChargeFraction);
}
