// Copyright Impact-Forge. ForgeVehicles damage adapter implementation.

#include "ForgeVehiclesDamageAdapterComponent.h"

#include "Engine/DamageEvents.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "UObject/UnrealType.h"

#if WITH_FORGEVEHICLES
#include "ForgeBaseVehicle.h"
#include "ForgeVehicleSeatConfig.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "Movement/ForgeVehicleTurretMovementComp.h"
#include "Vehicle/ForgeGroundVehicle.h"
#include "Vehicle/SuspensionComponent.h"

namespace
{
	// USuspensionComponent keeps its tuning UPROPERTYs private (AllowPrivateAccess
	// covers Blueprint only), so wheel damage reads/writes them via reflection -
	// stable because they are reflected properties, no ForgeVehicles edits needed.
	bool SetReflectedBool(UObject* Object, const FName PropertyName, const bool bValue)
	{
		if (!Object)
		{
			return false;
		}
		if (const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName))
		{
			Property->SetPropertyValue_InContainer(Object, bValue);
			return true;
		}
		return false;
	}

	TOptional<float> GetReflectedFloat(const UObject* Object, const FName PropertyName)
	{
		if (Object)
		{
			if (const FFloatProperty* Property = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName))
			{
				return Property->GetPropertyValue_InContainer(Object);
			}
		}
		return TOptional<float>();
	}

	const FName StiffnessPropertyName(TEXT("Stiffness"));
	const FName ZeroForcePropertyName(TEXT("bZeroForceComponent"));
}
#endif

UForgeVehiclesDamageAdapterComponent::UForgeVehiclesDamageAdapterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UForgeVehiclesDamageAdapterComponent::BeginPlay()
{
	Super::BeginPlay();

#if WITH_FORGEVEHICLES
	if (AForgeGroundVehicle* GroundVehicle = Cast<AForgeGroundVehicle>(GetOwner()))
	{
		const TArray<USuspensionComponent*> Suspension = GroundVehicle->GetSuspension();
		BaseWheelStiffness.Reset(Suspension.Num());
		for (const USuspensionComponent* Wheel : Suspension)
		{
			BaseWheelStiffness.Add(GetReflectedFloat(Wheel, StiffnessPropertyName).Get(22500.f));
		}
	}
	if (GetOwner())
	{
		if (const UForgeVehicleTurretMovementComp* Turret = GetOwner()->FindComponentByClass<UForgeVehicleTurretMovementComp>())
		{
			BaseTurretRotationRate = Turret->RotationRate;
		}
	}
#endif
}

void UForgeVehiclesDamageAdapterComponent::ApplyEngineState_Implementation(const float PowerScale, const bool bKilled)
{
#if WITH_FORGEVEHICLES
	if (bKilled)
	{
		if (IForgeVehicleMovementInterface* Movement = Cast<IForgeVehicleMovementInterface>(GetOwner()))
		{
			Movement->StopEngine();
		}
	}
	// Partial power degradation is vehicle specific (per-module torque tables);
	// Blueprint subclasses can extend this event with a richer mapping.
#endif
}

void UForgeVehiclesDamageAdapterComponent::ApplyWheelState_Implementation(const int32 WheelIndex, const float StiffnessScale, const bool bDisabled)
{
#if WITH_FORGEVEHICLES
	AForgeGroundVehicle* GroundVehicle = Cast<AForgeGroundVehicle>(GetOwner());
	if (!GroundVehicle)
	{
		return;
	}
	const TArray<USuspensionComponent*> Suspension = GroundVehicle->GetSuspension();
	if (!Suspension.IsValidIndex(WheelIndex) || !Suspension[WheelIndex])
	{
		return;
	}
	USuspensionComponent* Wheel = Suspension[WheelIndex];
	if (bDisabled)
	{
		SetReflectedBool(Wheel, ZeroForcePropertyName, true);
	}
	else if (BaseWheelStiffness.IsValidIndex(WheelIndex))
	{
		// Shot-up tires sag: reduced spring stiffness drags the corner down.
		Wheel->SetStiffness(BaseWheelStiffness[WheelIndex] * FMath::Clamp(StiffnessScale, 0.25f, 1.f));
	}
#endif
}

void UForgeVehiclesDamageAdapterComponent::ApplyTrackState_Implementation(const bool bLeftTrack, const bool bDisabled)
{
#if WITH_FORGEVEHICLES
	AForgeGroundVehicle* GroundVehicle = Cast<AForgeGroundVehicle>(GetOwner());
	if (!GroundVehicle || !bDisabled)
	{
		return;
	}
	// Tracked vehicles approximate tracks with a row of suspension components;
	// throwing a track zero-forces that entire side.
	for (USuspensionComponent* Wheel : GroundVehicle->GetSuspension())
	{
		if (Wheel && Wheel->IsLeftWheel() == bLeftTrack)
		{
			SetReflectedBool(Wheel, ZeroForcePropertyName, true);
		}
	}
#endif
}

void UForgeVehiclesDamageAdapterComponent::ApplyTurretDriveState_Implementation(const float TraverseScale, const float ElevationScale)
{
#if WITH_FORGEVEHICLES
	if (!GetOwner())
	{
		return;
	}
	if (UForgeVehicleTurretMovementComp* Turret = GetOwner()->FindComponentByClass<UForgeVehicleTurretMovementComp>())
	{
		FRotator NewRate = BaseTurretRotationRate;
		NewRate.Yaw *= FMath::Clamp(TraverseScale, 0.f, 1.f);
		NewRate.Pitch *= FMath::Clamp(ElevationScale, 0.f, 1.f);
		Turret->RotationRate = NewRate;
	}
#endif
}

void UForgeVehiclesDamageAdapterComponent::ApplyGunState_Implementation(const bool bCanFire)
{
	// ForgeVehicles carries no armament abstraction yet; Blueprint vehicles
	// bind this event to their weapon logic.
}

APawn* UForgeVehiclesDamageAdapterComponent::GetCrewOccupant_Implementation(const int32 SeatIndex) const
{
#if WITH_FORGEVEHICLES
	AForgeBaseVehicle* Vehicle = Cast<AForgeBaseVehicle>(GetOwner());
	if (!Vehicle)
	{
		return nullptr;
	}
	TArray<UForgeVehicleSeatConfig*> Seats;
	Vehicle->GetAllSeats(Seats);
	if (!Seats.IsValidIndex(SeatIndex) || !Seats[SeatIndex])
	{
		return nullptr;
	}
	if (const APlayerState* PlayerInSeat = Seats[SeatIndex]->PlayerInSeat)
	{
		return PlayerInSeat->GetPawn();
	}
#endif
	return nullptr;
}

void UForgeVehiclesDamageAdapterComponent::ApplyCrewWound_Implementation(const FForgeCrewWound& Wound)
{
	APawn* Occupant = IForgeVehicleDamageAdapter::Execute_GetCrewOccupant(this, Wound.SeatIndex);
	if (!Occupant)
	{
		return;
	}
	// Generic engine damage; games with richer pipelines (GAS medical systems)
	// should subclass and override this event.
	const float Damage = Wound.bLethalRoll ? 100000.f : Wound.EnergyJ * CrewDamagePerJoule;
	FDamageEvent DamageEvent;
	Occupant->TakeDamage(Damage, DamageEvent, Wound.InstigatorController, GetOwner());
}

void UForgeVehiclesDamageAdapterComponent::NotifyKillStateChanged_Implementation(const EForgeVehicleKillState NewState)
{
#if WITH_FORGEVEHICLES
	if (NewState == EForgeVehicleKillState::Destroyed || NewState == EForgeVehicleKillState::DestroyedCatastrophic)
	{
		if (IForgeVehicleMovementInterface* Movement = Cast<IForgeVehicleMovementInterface>(GetOwner()))
		{
			Movement->StopEngine();
		}
	}
#endif
}
