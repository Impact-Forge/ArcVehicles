// Copyright Impact-Forge. Vehicle damage controller implementation.

#include "Components/ForgeVehicleDamageComponent.h"

#include "Ballistics/ForgeArmorBallistics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/ForgeVehicleDamageModel.h"
#include "Engine/World.h"
#include "ForgeArmorCore.h"
#include "ForgeArmorSettings.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/ForgeVehicleDamageAdapter.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

void FForgeModuleStateArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 /*FinalSize*/)
{
	if (UForgeVehicleDamageComponent* DamageComponent = Owner.Get())
	{
		for (const int32 Index : ChangedIndices)
		{
			if (Items.IsValidIndex(Index))
			{
				DamageComponent->OnModuleStateReplicated(Items[Index]);
			}
		}
	}
}

UForgeVehicleDamageComponent::UForgeVehicleDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UForgeVehicleDamageComponent::BeginPlay()
{
	Super::BeginPlay();

	ModuleStates.Owner = this;
	CachedAdapter = ResolveAdapter();

	if (GetOwner() && GetOwner()->HasAuthority() && DamageModel)
	{
		StructuralHP = DamageModel->StructuralMaxHP;
		for (const FForgeDamageModuleDef& Def : DamageModel->Modules)
		{
			FForgeModuleRuntimeState& State = ModuleStates.Items.AddDefaulted_GetRef();
			State.ModuleId = Def.ModuleId;
			ModuleStates.MarkItemDirty(State);
		}
	}
}

void UForgeVehicleDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
		for (TPair<FName, FTimerHandle>& Pair : CookOffTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	CookOffTimers.Reset();
	Super::EndPlay(EndPlayReason);
}

void UForgeVehicleDamageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeVehicleDamageComponent, ModuleStates);
	DOREPLIFETIME(UForgeVehicleDamageComponent, KillState);
	DOREPLIFETIME(UForgeVehicleDamageComponent, StructuralHP);
}

UObject* UForgeVehicleDamageComponent::ResolveAdapter() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	if (Owner->GetClass()->ImplementsInterface(UForgeVehicleDamageAdapter::StaticClass()))
	{
		return Owner;
	}
	return Owner->FindComponentByInterface(UForgeVehicleDamageAdapter::StaticClass());
}

float UForgeVehicleDamageComponent::StateToEffectScale(const EForgeModuleState State)
{
	switch (State)
	{
	case EForgeModuleState::Nominal:   return 1.f;
	case EForgeModuleState::Damaged:   return 0.6f;
	case EForgeModuleState::Critical:  return 0.3f;
	case EForgeModuleState::Destroyed: return 0.f;
	default:                           return 1.f;
	}
}

FTransform UForgeVehicleDamageComponent::ResolveModuleTransform(const FForgeDamageModuleDef& Def) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return Def.LocalTransform;
	}
	if (!Def.AttachSocket.IsNone())
	{
		if (const USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (Mesh->DoesSocketExist(Def.AttachSocket))
			{
				return Def.LocalTransform * Mesh->GetSocketTransform(Def.AttachSocket);
			}
		}
	}
	return Def.LocalTransform * Owner->GetActorTransform();
}

FForgeModuleRuntimeState* UForgeVehicleDamageComponent::FindModuleState(const FName& ModuleId)
{
	return ModuleStates.Items.FindByPredicate([&ModuleId](const FForgeModuleRuntimeState& State) { return State.ModuleId == ModuleId; });
}

const FForgeModuleRuntimeState* UForgeVehicleDamageComponent::FindModuleState(const FName& ModuleId) const
{
	return ModuleStates.Items.FindByPredicate([&ModuleId](const FForgeModuleRuntimeState& State) { return State.ModuleId == ModuleId; });
}

bool UForgeVehicleDamageComponent::GetModuleState(const FName ModuleId, FForgeModuleRuntimeState& OutState) const
{
	if (const FForgeModuleRuntimeState* State = FindModuleState(ModuleId))
	{
		OutState = *State;
		return true;
	}
	return false;
}

float UForgeVehicleDamageComponent::GetStructuralHPFraction() const
{
	return DamageModel && DamageModel->StructuralMaxHP > 0.f ? FMath::Clamp(StructuralHP / DamageModel->StructuralMaxHP, 0.f, 1.f) : 1.f;
}

// ---------------------------------------------------------------- interior model

void UForgeVehicleDamageComponent::ProcessBehindArmor(const FForgeInteriorEvent& Event)
{
	using namespace ForgeArmor::Ballistics;

	if (!GetOwner() || !GetOwner()->HasAuthority() || !DamageModel || Event.TotalEnergyJ <= 0.0 || Event.FragmentCount <= 0)
	{
		return;
	}

	const double ConeSolidAngle = FMath::Max(ConeSolidAngleSr(Event.ConeHalfAngleDeg), 0.01);
	const double PerFragmentEnergy = Event.TotalEnergyJ / Event.FragmentCount;
	const FVector Direction = Event.DirectionWS.GetSafeNormal();

	// Structural pool absorbs the whole event.
	StructuralHP -= (float)(Event.TotalEnergyJ * 0.01);

	for (const FForgeDamageModuleDef& Def : DamageModel->Modules)
	{
		FForgeModuleRuntimeState* State = FindModuleState(Def.ModuleId);
		if (!State || State->State == EForgeModuleState::Destroyed)
		{
			continue;
		}

		const FTransform ModuleTransform = ResolveModuleTransform(Def);
		const FVector ToModule = ModuleTransform.GetLocation() - Event.OriginWS;
		const double Distance = ToModule.Size();
		if (Distance < 1.0)
		{
			// Point blank: the module eats a full share.
			ApplyModuleEnergy(Def, *State, PerFragmentEnergy * 2.0, PerFragmentEnergy, Event.bIncendiary, Event.InstigatorController);
			continue;
		}

		const double AngleToModuleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, ToModule / Distance), -1.0, 1.0)));
		// Allow the module's angular radius on top of the cone half angle.
		const double ModuleAngularRadiusDeg = FMath::RadiansToDegrees(FMath::Atan2(Def.VolumeExtentCm.GetMax(), Distance));
		if (AngleToModuleDeg > Event.ConeHalfAngleDeg + ModuleAngularRadiusDeg)
		{
			continue;
		}

		const double ModuleSolidAngle = ApproxSolidAngleSr(Def.VolumeExtentCm, Distance);
		const double ExpectedHits = FMath::Clamp(Event.FragmentCount * ModuleSolidAngle / ConeSolidAngle, 0.0, (double)Event.FragmentCount);
		if (ExpectedHits < 0.25)
		{
			continue;
		}

		const double DeliveredEnergy = ExpectedHits * PerFragmentEnergy;
		if (Def.Type == EForgeModuleType::CrewSeat)
		{
			ApplyCrewHit(Def, *State, DeliveredEnergy, Event.InstigatorController);
		}
		else
		{
			ApplyModuleEnergy(Def, *State, DeliveredEnergy, PerFragmentEnergy, Event.bIncendiary, Event.InstigatorController);
		}
	}

	UpdateKillState();
}

void UForgeVehicleDamageComponent::ProcessAPHEBurst(const FVector& OriginWS, const float ChargeGrams, AController* InstigatorController, AActor* DamageCauser)
{
	using namespace ForgeArmor::Ballistics;

	if (!GetOwner() || !GetOwner()->HasAuthority() || !DamageModel)
	{
		return;
	}

	const double TNTKg = ChargeGrams / 1000.0;
	const double BlastEnergy = BlastEnergyJ(TNTKg) * 0.05; // fraction coupling into fragments/overpressure
	const double RadiusCm = 250.0 * FMath::Pow(FMath::Max(TNTKg, 0.001), 1.0 / 3.0);

	StructuralHP -= (float)(BlastEnergy * 0.02);

	for (const FForgeDamageModuleDef& Def : DamageModel->Modules)
	{
		FForgeModuleRuntimeState* State = FindModuleState(Def.ModuleId);
		if (!State || State->State == EForgeModuleState::Destroyed)
		{
			continue;
		}
		const double Distance = FVector::Dist(ResolveModuleTransform(Def).GetLocation(), OriginWS);
		if (Distance > RadiusCm)
		{
			continue;
		}
		const double Falloff = 1.0 - Distance / RadiusCm;
		const double Delivered = BlastEnergy * Falloff * 0.25;
		if (Def.Type == EForgeModuleType::CrewSeat)
		{
			ApplyCrewHit(Def, *State, Delivered, InstigatorController);
		}
		else
		{
			ApplyModuleEnergy(Def, *State, Delivered, Delivered, /*bIncendiary*/ true, InstigatorController);
		}
	}

	OnAmmoDetonation.Broadcast(OriginWS); // reuse the big-boom FX surface for interior bursts
	UpdateKillState();
}

void UForgeVehicleDamageComponent::ProcessExternalHit(const FVector& OriginWS, const double EnergyJ, const double RadiusCm, AController* InstigatorController, AActor* DamageCauser)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DamageModel || EnergyJ <= 0.0)
	{
		return;
	}

	for (const FForgeDamageModuleDef& Def : DamageModel->Modules)
	{
		if (!Def.bExternal)
		{
			continue;
		}
		FForgeModuleRuntimeState* State = FindModuleState(Def.ModuleId);
		if (!State || State->State == EForgeModuleState::Destroyed)
		{
			continue;
		}
		const double Distance = FVector::Dist(ResolveModuleTransform(Def).GetLocation(), OriginWS);
		if (Distance > RadiusCm)
		{
			continue;
		}
		ApplyModuleEnergy(Def, *State, EnergyJ * (1.0 - Distance / RadiusCm), EnergyJ, false, InstigatorController);
	}

	UpdateKillState();
}

void UForgeVehicleDamageComponent::ApplyBlast(const FVector& OriginWS, const float TNTEquivalentKG, const float RadiusCm, AController* InstigatorController, AActor* DamageCauser)
{
	using namespace ForgeArmor::Ballistics;

	if (!GetOwner() || !GetOwner()->HasAuthority() || !DamageModel)
	{
		return;
	}

	const double BlastEnergy = BlastEnergyJ(TNTEquivalentKG) * 0.02;
	const bool bReachesInterior = DamageModel->bOpenTop;

	StructuralHP -= (float)(BlastEnergy * 0.01);

	for (const FForgeDamageModuleDef& Def : DamageModel->Modules)
	{
		if (!Def.bExternal && !bReachesInterior)
		{
			continue;
		}
		FForgeModuleRuntimeState* State = FindModuleState(Def.ModuleId);
		if (!State || State->State == EForgeModuleState::Destroyed)
		{
			continue;
		}
		const double Distance = FVector::Dist(ResolveModuleTransform(Def).GetLocation(), OriginWS);
		if (Distance > RadiusCm)
		{
			continue;
		}
		const double Falloff = FMath::Square(1.0 - Distance / RadiusCm);
		const double Delivered = BlastEnergy * Falloff * 0.2;
		if (Def.Type == EForgeModuleType::CrewSeat)
		{
			ApplyCrewHit(Def, *State, Delivered, InstigatorController);
		}
		else
		{
			ApplyModuleEnergy(Def, *State, Delivered, Delivered, false, InstigatorController);
		}
	}

	UpdateKillState();
}

void UForgeVehicleDamageComponent::ApplyModuleDamage(const FName ModuleId, const float DamageHP, AController* InstigatorController)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DamageModel)
	{
		return;
	}
	const FForgeDamageModuleDef* Def = DamageModel->FindModule(ModuleId);
	FForgeModuleRuntimeState* State = FindModuleState(ModuleId);
	if (!Def || !State || Def->MaxHP <= 0.f)
	{
		return;
	}
	const double EnergyEquivalent = (DamageHP / Def->MaxHP) * Def->EnergyCapacityJ;
	ApplyModuleEnergy(*Def, *State, EnergyEquivalent, EnergyEquivalent, false, InstigatorController);
	UpdateKillState();
}

void UForgeVehicleDamageComponent::ApplyModuleEnergy(const FForgeDamageModuleDef& Def, FForgeModuleRuntimeState& State, const double EnergyJ, const double PerFragmentEnergyJ, const bool bIncendiary, AController* InstigatorController)
{
	if (EnergyJ <= 0.0 || Def.EnergyCapacityJ <= 0.f)
	{
		return;
	}

	const double DamageFraction = EnergyJ / Def.EnergyCapacityJ;
	State.HPFraction = FMath::Clamp(State.HPFraction - (float)DamageFraction, 0.f, 1.f);

	EForgeModuleState NewState = EForgeModuleState::Nominal;
	if (State.HPFraction <= 0.f)
	{
		NewState = EForgeModuleState::Destroyed;
	}
	else if (State.HPFraction <= Def.CriticalThreshold)
	{
		NewState = EForgeModuleState::Critical;
	}
	else if (State.HPFraction <= Def.DamagedThreshold)
	{
		NewState = EForgeModuleState::Damaged;
	}

	const bool bStateChanged = NewState != State.State;
	State.State = NewState;

	// Fire ignition roll: flammable modules can catch when meaningfully damaged.
	if (!State.bOnFire && Def.Flammability > 0.f && DamageFraction > 0.05)
	{
		const float IncendiaryScale = bIncendiary ? 2.f : 1.f;
		if (FMath::FRand() < Def.Flammability * IncendiaryScale * FMath::Min(DamageFraction * 2.0, 1.0))
		{
			SetModuleOnFire(Def, State);
		}
	}

	// Destroyed ammo stowage detonates immediately on a heavy hit.
	if (bStateChanged && NewState == EForgeModuleState::Destroyed && Def.Type == EForgeModuleType::AmmoStowage)
	{
		const float DetonationChance = Def.bWetStowage ? 0.35f : 0.75f;
		if (FMath::FRand() < DetonationChance)
		{
			DetonateAmmoModule(Def.ModuleId);
			return; // detonation already marked everything dirty
		}
	}

	ModuleStates.MarkItemDirty(State);
	if (bStateChanged)
	{
		ApplyAdapterEffects(Def, State);
		OnModuleStateChanged.Broadcast(State);

		// Dependency chain: destroying this module drags dependents down.
		if (NewState == EForgeModuleState::Destroyed && DamageModel)
		{
			for (const FForgeDamageModuleDef& Other : DamageModel->Modules)
			{
				if (Other.DisablesWith.Contains(Def.ModuleId))
				{
					if (FForgeModuleRuntimeState* OtherState = FindModuleState(Other.ModuleId))
					{
						if (OtherState->State != EForgeModuleState::Destroyed)
						{
							OtherState->HPFraction = 0.f;
							OtherState->State = EForgeModuleState::Destroyed;
							ModuleStates.MarkItemDirty(*OtherState);
							ApplyAdapterEffects(Other, *OtherState);
							OnModuleStateChanged.Broadcast(*OtherState);
						}
					}
				}
			}
		}
	}
}

void UForgeVehicleDamageComponent::ApplyCrewHit(const FForgeDamageModuleDef& Def, FForgeModuleRuntimeState& State, const double EnergyJ, AController* InstigatorController)
{
	const UForgeArmorSettings* Settings = UForgeArmorSettings::Get();
	if (EnergyJ < Settings->CrewIncapacitationEnergyJ * 0.5)
	{
		return;
	}

	FForgeCrewWound Wound;
	Wound.SeatModuleId = Def.ModuleId;
	Wound.SeatIndex = Def.SeatIndex;
	Wound.CrewRoleTag = Def.CrewRoleTag;
	Wound.EnergyJ = (float)EnergyJ;
	Wound.InstigatorController = InstigatorController;

	if (EnergyJ >= Settings->CrewLethalEnergyJ)
	{
		const float LethalChance = FMath::Clamp((float)(EnergyJ / Settings->CrewLethalEnergyJ) - 1.f, 0.15f, 0.9f);
		Wound.bLethalRoll = FMath::FRand() < LethalChance;
	}

	if (CachedAdapter)
	{
		IForgeVehicleDamageAdapter::Execute_ApplyCrewWound(CachedAdapter, Wound);
	}
	OnCrewWound.Broadcast(Wound);

	// Track station status: a killed occupant marks the seat module destroyed.
	if (Wound.bLethalRoll)
	{
		State.HPFraction = 0.f;
		State.State = EForgeModuleState::Destroyed;
		ModuleStates.MarkItemDirty(State);
		OnModuleStateChanged.Broadcast(State);
	}
	else if (EnergyJ >= Settings->CrewIncapacitationEnergyJ && State.State == EForgeModuleState::Nominal)
	{
		State.HPFraction = FMath::Min(State.HPFraction, 0.5f);
		State.State = EForgeModuleState::Damaged;
		ModuleStates.MarkItemDirty(State);
		OnModuleStateChanged.Broadcast(State);
	}
}

void UForgeVehicleDamageComponent::ApplyAdapterEffects(const FForgeDamageModuleDef& Def, const FForgeModuleRuntimeState& State)
{
	if (!CachedAdapter)
	{
		CachedAdapter = ResolveAdapter();
		if (!CachedAdapter)
		{
			return;
		}
	}

	const float Scale = StateToEffectScale(State.State);
	const bool bDestroyed = State.State == EForgeModuleState::Destroyed;

	switch (Def.Type)
	{
	case EForgeModuleType::Engine:
		IForgeVehicleDamageAdapter::Execute_ApplyEngineState(CachedAdapter, Scale, bDestroyed);
		break;
	case EForgeModuleType::Transmission:
		IForgeVehicleDamageAdapter::Execute_ApplyEngineState(CachedAdapter, FMath::Min(Scale, 0.8f), bDestroyed);
		break;
	case EForgeModuleType::Wheel:
		IForgeVehicleDamageAdapter::Execute_ApplyWheelState(CachedAdapter, Def.WheelIndex, Scale, bDestroyed);
		break;
	case EForgeModuleType::TrackLeft:
		IForgeVehicleDamageAdapter::Execute_ApplyTrackState(CachedAdapter, true, bDestroyed);
		break;
	case EForgeModuleType::TrackRight:
		IForgeVehicleDamageAdapter::Execute_ApplyTrackState(CachedAdapter, false, bDestroyed);
		break;
	case EForgeModuleType::TurretTraverse:
		IForgeVehicleDamageAdapter::Execute_ApplyTurretDriveState(CachedAdapter, Scale, Scale);
		break;
	case EForgeModuleType::GunBreech:
	case EForgeModuleType::GunBarrel:
		IForgeVehicleDamageAdapter::Execute_ApplyGunState(CachedAdapter, !bDestroyed);
		break;
	default:
		break;
	}
}

void UForgeVehicleDamageComponent::UpdateKillState()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DamageModel)
	{
		return;
	}
	if (KillState == EForgeVehicleKillState::Destroyed || KillState == EForgeVehicleKillState::DestroyedCatastrophic)
	{
		return;
	}

	bool bMobilityKill = false;
	bool bFirepowerKill = false;
	int32 DestroyedWheels = 0;
	int32 CrewSeats = 0;
	int32 DeadCrewSeats = 0;

	for (const FForgeDamageModuleDef& Def : DamageModel->Modules)
	{
		const FForgeModuleRuntimeState* State = FindModuleState(Def.ModuleId);
		if (!State)
		{
			continue;
		}
		const bool bDestroyed = State->State == EForgeModuleState::Destroyed;
		switch (Def.Type)
		{
		case EForgeModuleType::Engine:
		case EForgeModuleType::Transmission:
		case EForgeModuleType::TrackLeft:
		case EForgeModuleType::TrackRight:
			bMobilityKill |= bDestroyed;
			break;
		case EForgeModuleType::Wheel:
			DestroyedWheels += bDestroyed ? 1 : 0;
			break;
		case EForgeModuleType::GunBreech:
		case EForgeModuleType::GunBarrel:
		case EForgeModuleType::TurretTraverse:
			bFirepowerKill |= bDestroyed;
			break;
		case EForgeModuleType::CrewSeat:
			++CrewSeats;
			DeadCrewSeats += bDestroyed ? 1 : 0;
			break;
		default:
			break;
		}
	}

	if (DamageModel->WheelLossMobilityKillCount > 0 && DestroyedWheels >= DamageModel->WheelLossMobilityKillCount)
	{
		bMobilityKill = true;
	}

	EForgeVehicleKillState NewState = EForgeVehicleKillState::Operational;
	if (StructuralHP <= 0.f || (CrewSeats > 0 && DeadCrewSeats >= CrewSeats))
	{
		NewState = EForgeVehicleKillState::Destroyed;
	}
	else if (bMobilityKill && bFirepowerKill)
	{
		NewState = EForgeVehicleKillState::MobilityAndFirepowerKill;
	}
	else if (bMobilityKill)
	{
		NewState = EForgeVehicleKillState::MobilityKill;
	}
	else if (bFirepowerKill)
	{
		NewState = EForgeVehicleKillState::FirepowerKill;
	}

	if (NewState != KillState)
	{
		KillState = NewState;
		if (CachedAdapter)
		{
			IForgeVehicleDamageAdapter::Execute_NotifyKillStateChanged(CachedAdapter, KillState);
		}
		OnKillStateChanged.Broadcast(KillState);
	}
}

// ---------------------------------------------------------------- fire & cook-off

void UForgeVehicleDamageComponent::SetModuleOnFire(const FForgeDamageModuleDef& Def, FForgeModuleRuntimeState& State)
{
	if (State.bOnFire || !GetWorld())
	{
		return;
	}
	State.bOnFire = true;
	ModuleStates.MarkItemDirty(State);
	OnModuleFireChanged.Broadcast(Def.ModuleId, true);

	const UForgeArmorSettings* Settings = UForgeArmorSettings::Get();

	if (!FireTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &UForgeVehicleDamageComponent::FireTick, Settings->FireTickIntervalSeconds, true);
	}

	// Burning ammo cooks off after a randomized delay.
	if (Def.Type == EForgeModuleType::AmmoStowage && !CookOffTimers.Contains(Def.ModuleId))
	{
		const float BaseDelay = FMath::FRandRange(Settings->CookOffMinSeconds, Settings->CookOffMaxSeconds);
		const float Delay = Def.bWetStowage ? BaseDelay * 2.f : BaseDelay;
		FTimerHandle& Handle = CookOffTimers.Add(Def.ModuleId);
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UForgeVehicleDamageComponent::DetonateAmmoModule, Def.ModuleId);
		GetWorld()->GetTimerManager().SetTimer(Handle, Delegate, Delay, false);
	}
}

void UForgeVehicleDamageComponent::FireTick()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DamageModel)
	{
		return;
	}

	const UForgeArmorSettings* Settings = UForgeArmorSettings::Get();
	bool bAnyFire = false;

	for (const FForgeDamageModuleDef& Def : DamageModel->Modules)
	{
		FForgeModuleRuntimeState* State = FindModuleState(Def.ModuleId);
		if (!State || !State->bOnFire)
		{
			continue;
		}
		bAnyFire = true;

		// Burn damage to the module itself.
		if (State->State != EForgeModuleState::Destroyed)
		{
			ApplyModuleEnergy(Def, *State, Settings->FireDamageFractionPerTick * Def.EnergyCapacityJ, 0.0, false, nullptr);
		}

		const FVector FireLocation = ResolveModuleTransform(Def).GetLocation();

		// Crew in the burning compartment take burn wounds.
		for (const FForgeDamageModuleDef& CrewDef : DamageModel->Modules)
		{
			if (CrewDef.Type != EForgeModuleType::CrewSeat)
			{
				continue;
			}
			FForgeModuleRuntimeState* CrewState = FindModuleState(CrewDef.ModuleId);
			if (!CrewState || CrewState->State == EForgeModuleState::Destroyed)
			{
				continue;
			}
			if (FVector::Dist(ResolveModuleTransform(CrewDef).GetLocation(), FireLocation) <= DamageModel->FireSpreadRadiusCm)
			{
				ApplyCrewHit(CrewDef, *CrewState, Settings->FireCrewEnergyPerTickJ, nullptr);
			}
		}

		// Spread to nearby flammable modules.
		for (const FForgeDamageModuleDef& OtherDef : DamageModel->Modules)
		{
			if (OtherDef.ModuleId == Def.ModuleId || OtherDef.Flammability <= 0.f)
			{
				continue;
			}
			FForgeModuleRuntimeState* OtherState = FindModuleState(OtherDef.ModuleId);
			if (!OtherState || OtherState->bOnFire)
			{
				continue;
			}
			if (FVector::Dist(ResolveModuleTransform(OtherDef).GetLocation(), FireLocation) <= DamageModel->FireSpreadRadiusCm)
			{
				if (FMath::FRand() < OtherDef.Flammability * 0.1f)
				{
					SetModuleOnFire(OtherDef, *OtherState);
				}
			}
		}
	}

	if (!bAnyFire && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
		FireTimerHandle.Invalidate();
	}

	UpdateKillState();
}

void UForgeVehicleDamageComponent::ExtinguishFires()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	for (FForgeModuleRuntimeState& State : ModuleStates.Items)
	{
		if (State.bOnFire)
		{
			State.bOnFire = false;
			ModuleStates.MarkItemDirty(State);
			OnModuleFireChanged.Broadcast(State.ModuleId, false);
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
		FireTimerHandle.Invalidate();
		for (TPair<FName, FTimerHandle>& Pair : CookOffTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	CookOffTimers.Reset();
}

void UForgeVehicleDamageComponent::DetonateAmmoModule(const FName ModuleId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DamageModel)
	{
		return;
	}
	const FForgeDamageModuleDef* Def = DamageModel->FindModule(ModuleId);
	FForgeModuleRuntimeState* State = FindModuleState(ModuleId);
	if (!Def || !State)
	{
		return;
	}

	State->HPFraction = 0.f;
	State->State = EForgeModuleState::Destroyed;
	State->bOnFire = true;
	ModuleStates.MarkItemDirty(*State);
	OnModuleStateChanged.Broadcast(*State);

	const FVector Location = ResolveModuleTransform(*Def).GetLocation();
	OnAmmoDetonation.Broadcast(Location);

	if (DamageModel->bBlowOutPanels)
	{
		// Vented detonation: the fighting compartment survives, the gun does not.
		for (const FForgeDamageModuleDef& OtherDef : DamageModel->Modules)
		{
			if (OtherDef.Type == EForgeModuleType::GunBreech || OtherDef.Type == EForgeModuleType::TurretTraverse)
			{
				if (FForgeModuleRuntimeState* OtherState = FindModuleState(OtherDef.ModuleId))
				{
					OtherState->HPFraction = 0.f;
					OtherState->State = EForgeModuleState::Destroyed;
					ModuleStates.MarkItemDirty(*OtherState);
					ApplyAdapterEffects(OtherDef, *OtherState);
					OnModuleStateChanged.Broadcast(*OtherState);
				}
			}
		}
		UpdateKillState();
	}
	else
	{
		// Catastrophic loss: everyone and everything inside is gone.
		for (const FForgeDamageModuleDef& OtherDef : DamageModel->Modules)
		{
			FForgeModuleRuntimeState* OtherState = FindModuleState(OtherDef.ModuleId);
			if (!OtherState || OtherState->State == EForgeModuleState::Destroyed)
			{
				continue;
			}
			if (OtherDef.Type == EForgeModuleType::CrewSeat)
			{
				ApplyCrewHit(OtherDef, *OtherState, 100000.0, nullptr);
			}
			OtherState->HPFraction = 0.f;
			OtherState->State = EForgeModuleState::Destroyed;
			ModuleStates.MarkItemDirty(*OtherState);
			ApplyAdapterEffects(OtherDef, *OtherState);
		}

		StructuralHP = 0.f;
		KillState = EForgeVehicleKillState::DestroyedCatastrophic;
		if (CachedAdapter)
		{
			IForgeVehicleDamageAdapter::Execute_NotifyKillStateChanged(CachedAdapter, KillState);
		}
		OnKillStateChanged.Broadcast(KillState);
	}
}

// ---------------------------------------------------------------- FX / replication

void UForgeVehicleDamageComponent::BroadcastImpactFX(const FForgeArmorImpactFXEvent& FXEvent)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		MulticastImpactFX(FXEvent);
	}
}

void UForgeVehicleDamageComponent::MulticastImpactFX_Implementation(FForgeArmorImpactFXEvent FXEvent)
{
	OnImpactFX.Broadcast(FXEvent);
}

void UForgeVehicleDamageComponent::OnRep_KillState()
{
	if (UObject* Adapter = CachedAdapter ? CachedAdapter.Get() : ResolveAdapter())
	{
		CachedAdapter = Adapter;
		IForgeVehicleDamageAdapter::Execute_NotifyKillStateChanged(Adapter, KillState);
	}
	OnKillStateChanged.Broadcast(KillState);
}

void UForgeVehicleDamageComponent::OnModuleStateReplicated(const FForgeModuleRuntimeState& ModuleState)
{
	// Client-side: mirror functional consequences that affect local simulation
	// (turret traverse scaling, wheel visuals) and surface the change to BP.
	if (DamageModel)
	{
		if (const FForgeDamageModuleDef* Def = DamageModel->FindModule(ModuleState.ModuleId))
		{
			ApplyAdapterEffects(*Def, ModuleState);
		}
	}
	OnModuleStateChanged.Broadcast(ModuleState);
}
