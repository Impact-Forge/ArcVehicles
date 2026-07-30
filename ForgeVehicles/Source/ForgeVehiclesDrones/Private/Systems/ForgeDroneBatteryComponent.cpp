// Copyright Impact-Forge. Drone battery implementation.

#include "Systems/ForgeDroneBatteryComponent.h"

#include "Flight/ForgeMultirotorVehicle.h"
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

	const float PropulsionDemand = GatherPropulsionDemand();
	CurrentLoadW = AvionicsLoadW + PayloadLoadW + ForgeDrone::Battery::PropulsionLoadW(PropulsionDemand, MaxPropulsionLoadW);

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

float UForgeDroneBatteryComponent::GatherPropulsionDemand() const
{
	if (const AForgeMultirotorVehicle* Multirotor = Cast<AForgeMultirotorVehicle>(GetOwner()))
	{
		return Multirotor->GetMeanMotorOutput();
	}

	// Fixed-wing and other airframes report demand by pushing SetPayloadLoadW/driving this component
	// externally; without a known airframe assume idle rather than inventing a load.
	return 0.f;
}

void UForgeDroneBatteryComponent::ApplyPowerScaleToOwner(const float PowerScale) const
{
	if (AForgeMultirotorVehicle* Multirotor = Cast<AForgeMultirotorVehicle>(GetOwner()))
	{
		Multirotor->SetPowerScale(PowerScale);
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
