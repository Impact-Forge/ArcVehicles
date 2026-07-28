// Copyright Impact-Forge. BSP damage adapter implementation.

#include "ForgeArmorBSPAdapterComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "Components/ForgeVehicleDamageComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "UObject/UnrealType.h"

#if WITH_BSPPLATFORMS
#include "Vehicles/BSP_VehicleBase.h"
#include "Vehicles/Components/BSP_EngineComponent.h"
#include "Vehicles/Components/BSP_Seat.h"
#include "Vehicles/Components/BSP_Turret.h"
#include "Vehicles/Components/BSP_WheelComponent.h"
#endif

namespace
{
	// RTune's suspension tuning UPROPERTYs are private (AllowPrivateAccess covers
	// Blueprint only); wheel damage flips them via property reflection instead.
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

	bool SetReflectedFloat(UObject* Object, const FName PropertyName, const float Value)
	{
		if (!Object)
		{
			return false;
		}
		if (const FFloatProperty* Property = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName))
		{
			Property->SetPropertyValue_InContainer(Object, Value);
			return true;
		}
		return false;
	}

	UAbilitySystemComponent* GetASCFromActor(AActor* Actor)
	{
		if (const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Actor))
		{
			return Interface->GetAbilitySystemComponent();
		}
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	}

	void ApplySetByCallerEffect(UAbilitySystemComponent* ASC, const TSubclassOf<UGameplayEffect>& EffectClass, const FGameplayTag& SetByCallerTag, const float Magnitude)
	{
		if (!ASC || !EffectClass)
		{
			return;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (SpecHandle.IsValid())
		{
			if (SetByCallerTag.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(SetByCallerTag, Magnitude);
			}
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
		}
	}

#if WITH_BSPPLATFORMS
	const FName ZeroForcePropertyName(TEXT("bZeroForceComponent"));
	const FName BaseTurnRatePropertyName(TEXT("BaseTurnRate"));
	const FName MantletTurnRatePropertyName(TEXT("MantletTurnRate"));

	ABSP_Turret* FindTurret(const AActor* Owner)
	{
		if (!Owner)
		{
			return nullptr;
		}
		TArray<AActor*> Attached;
		Owner->GetAttachedActors(Attached);
		for (AActor* Actor : Attached)
		{
			if (ABSP_Turret* Turret = Cast<ABSP_Turret>(Actor))
			{
				return Turret;
			}
		}
		return nullptr;
	}
#endif
}

UForgeArmorBSPAdapterComponent::UForgeArmorBSPAdapterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UForgeArmorBSPAdapterComponent::BeginPlay()
{
	Super::BeginPlay();

#if WITH_BSPPLATFORMS
	if (const ABSP_Turret* Turret = FindTurret(GetOwner()))
	{
		BaseTurretTurnRate = GetReflectedFloat(Turret, BaseTurnRatePropertyName).Get(45.f);
		BaseMantletTurnRate = GetReflectedFloat(Turret, MantletTurnRatePropertyName).Get(30.f);
	}
#endif

	// Mirror aggregate structural damage into the vehicle's Health attribute.
	if (UForgeVehicleDamageComponent* Damage = GetOwner() ? GetOwner()->FindComponentByClass<UForgeVehicleDamageComponent>() : nullptr)
	{
		Damage->OnModuleStateChanged.AddDynamic(this, &UForgeArmorBSPAdapterComponent::HandleModuleStateChanged);
		LastStructuralFraction = Damage->GetStructuralHPFraction();
	}
}

void UForgeArmorBSPAdapterComponent::HandleModuleStateChanged(const FForgeModuleRuntimeState& ModuleState)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (const UForgeVehicleDamageComponent* Damage = GetOwner()->FindComponentByClass<UForgeVehicleDamageComponent>())
	{
		const float CurrentFraction = Damage->GetStructuralHPFraction();
		if (CurrentFraction < LastStructuralFraction)
		{
			PushHullDamage((LastStructuralFraction - CurrentFraction) * HullHealthPerStructuralLoss);
			LastStructuralFraction = CurrentFraction;
		}
	}
}

void UForgeArmorBSPAdapterComponent::PushHullDamage(const float Magnitude)
{
	if (Magnitude <= 0.f)
	{
		return;
	}
	ApplySetByCallerEffect(GetASCFromActor(GetOwner()), HullDamageEffect, HullDamageSetByCallerTag, Magnitude);
}

void UForgeArmorBSPAdapterComponent::ApplyEngineState_Implementation(const float PowerScale, const bool bKilled)
{
#if WITH_BSPPLATFORMS
	if (!bKilled || !GetOwner())
	{
		return;
	}
	if (UBSP_EngineComponent* Engine = GetOwner()->FindComponentByClass<UBSP_EngineComponent>())
	{
		// The ignition-stop entry point is private C++ but a UFUNCTION - invoke it
		// through the reflection call path.
		if (UFunction* StopFunction = Engine->FindFunction(TEXT("EngineIgnitionStop")))
		{
			Engine->ProcessEvent(StopFunction, nullptr);
		}
	}
#endif
}

void UForgeArmorBSPAdapterComponent::ApplyWheelState_Implementation(const int32 WheelIndex, const float StiffnessScale, const bool bDisabled)
{
#if WITH_BSPPLATFORMS
	if (!GetOwner() || !bDisabled)
	{
		return;
	}
	TArray<UBSP_WheelComponent*> Wheels;
	GetOwner()->GetComponents<UBSP_WheelComponent>(Wheels);
	if (Wheels.IsValidIndex(WheelIndex))
	{
		SetReflectedBool(Wheels[WheelIndex], ZeroForcePropertyName, true);
	}
#endif
}

void UForgeArmorBSPAdapterComponent::ApplyTrackState_Implementation(const bool bLeftTrack, const bool bDisabled)
{
#if WITH_BSPPLATFORMS
	if (!GetOwner() || !bDisabled)
	{
		return;
	}
	TArray<UBSP_WheelComponent*> Wheels;
	GetOwner()->GetComponents<UBSP_WheelComponent>(Wheels);
	const FTransform OwnerTransform = GetOwner()->GetActorTransform();
	for (UBSP_WheelComponent* Wheel : Wheels)
	{
		if (!Wheel)
		{
			continue;
		}
		const bool bIsLeft = OwnerTransform.InverseTransformPosition(Wheel->GetComponentLocation()).Y < 0.f;
		if (bIsLeft == bLeftTrack)
		{
			SetReflectedBool(Wheel, ZeroForcePropertyName, true);
		}
	}
#endif
}

void UForgeArmorBSPAdapterComponent::ApplyTurretDriveState_Implementation(const float TraverseScale, const float ElevationScale)
{
#if WITH_BSPPLATFORMS
	if (ABSP_Turret* Turret = FindTurret(GetOwner()))
	{
		SetReflectedFloat(Turret, BaseTurnRatePropertyName, BaseTurretTurnRate * FMath::Clamp(TraverseScale, 0.f, 1.f));
		SetReflectedFloat(Turret, MantletTurnRatePropertyName, BaseMantletTurnRate * FMath::Clamp(ElevationScale, 0.f, 1.f));
	}
#endif
}

void UForgeArmorBSPAdapterComponent::ApplyGunState_Implementation(const bool bCanFire)
{
	// BSP turret firing is Blueprint-driven; vehicle Blueprints bind this event
	// to block FireWeapon when the breech/barrel is destroyed.
}

APawn* UForgeArmorBSPAdapterComponent::GetCrewOccupant_Implementation(const int32 SeatIndex) const
{
#if WITH_BSPPLATFORMS
	const ABSP_VehicleBase* Vehicle = Cast<ABSP_VehicleBase>(GetOwner());
	if (!Vehicle || !Vehicle->SeatsArray.IsValidIndex(SeatIndex) || !Vehicle->SeatsArray[SeatIndex])
	{
		return nullptr;
	}
	return Cast<APawn>(Vehicle->SeatsArray[SeatIndex]->GetCurrentSeatOccupant());
#else
	return nullptr;
#endif
}

void UForgeArmorBSPAdapterComponent::ApplyCrewWound_Implementation(const FForgeCrewWound& Wound)
{
	APawn* Occupant = IForgeVehicleDamageAdapter::Execute_GetCrewOccupant(this, Wound.SeatIndex);
	if (!Occupant)
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = GetASCFromActor(Occupant))
	{
		// Lethal rolls deliver an overwhelming magnitude so the character's damage
		// pipeline resolves the kill through its normal path.
		const float Magnitude = Wound.bLethalRoll ? 100000.f : Wound.EnergyJ;
		ApplySetByCallerEffect(ASC, CrewWoundEffect, CrewWoundSetByCallerTag, Magnitude);
	}
}

void UForgeArmorBSPAdapterComponent::NotifyKillStateChanged_Implementation(const EForgeVehicleKillState NewState)
{
	if (NewState == EForgeVehicleKillState::Destroyed || NewState == EForgeVehicleKillState::DestroyedCatastrophic)
	{
		IForgeVehicleDamageAdapter::Execute_ApplyEngineState(this, 0.f, true);
	}
}
