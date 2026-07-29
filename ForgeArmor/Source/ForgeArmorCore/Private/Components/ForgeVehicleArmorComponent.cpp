// Copyright Impact-Forge. Vehicle armor orchestrator implementation.

#include "Components/ForgeVehicleArmorComponent.h"

#include "Ballistics/ForgeArmorBallistics.h"
#include "Components/ForgeArmorZoneComponent.h"
#include "Components/ForgeVehicleDamageComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/ForgeArmorMaterialSet.h"
#include "Data/ForgeArmorProfile.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "ForgeArmorCore.h"
#include "ForgeArmorSettings.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Subsystems/ForgeArmorRegistrySubsystem.h"

namespace
{
	TAutoConsoleVariable<int32> CVarForgeArmorDebug(
		TEXT("forge.Armor.Debug"),
		0,
		TEXT("Draw Forge Armor penetration solves (1 = lines, 2 = verbose)."),
		ECVF_Cheat);

	bool ShouldDebugDraw()
	{
		return CVarForgeArmorDebug.GetValueOnGameThread() > 0 || UForgeArmorSettings::Get()->bDebugDraw;
	}

	uint8 QuantizeCaliberClass(const double CaliberMM)
	{
		if (CaliberMM < 10.0)  { return 0; } // small arms
		if (CaliberMM < 16.0)  { return 1; } // HMG
		if (CaliberMM < 41.0)  { return 2; } // autocannon
		if (CaliberMM < 91.0)  { return 3; } // cannon
		return 4;                            // large caliber
	}

	// Minimum residual speeds worth continuing the projectile for (m/s).
	constexpr double MinContinuationSpeedMS = 40.0;
	constexpr double MinRicochetSpeedMS = 30.0;
}

void FForgeArmorZoneStateArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 /*FinalSize*/)
{
	if (UForgeVehicleArmorComponent* ArmorComponent = Owner.Get())
	{
		for (const int32 Index : ChangedIndices)
		{
			if (Items.IsValidIndex(Index))
			{
				ArmorComponent->HandleZoneStateReplicated(Items[Index]);
			}
		}
	}
}

UForgeVehicleArmorComponent::UForgeVehicleArmorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UForgeVehicleArmorComponent::BeginPlay()
{
	Super::BeginPlay();

	ZoneStates.Owner = this;
	CachedMaterialSet = const_cast<UForgeArmorMaterialSet*>(GetMaterialSet());

	SpawnZones();

	if (UForgeArmorRegistrySubsystem* Registry = UForgeArmorRegistrySubsystem::Get(this))
	{
		Registry->RegisterArmor(this);
	}

	if (bAutoArm && UForgeArmorSettings::Get()->bArmorSystemEnabled)
	{
		SetArmorArmed(true);
	}
}

void UForgeVehicleArmorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UForgeArmorRegistrySubsystem* Registry = UForgeArmorRegistrySubsystem::Get(this))
	{
		Registry->UnregisterArmor(this);
	}
	DestroyZones();
	Super::EndPlay(EndPlayReason);
}

void UForgeVehicleArmorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeVehicleArmorComponent, ZoneStates);
}

const UForgeArmorMaterialSet* UForgeVehicleArmorComponent::GetMaterialSet() const
{
	if (MaterialSetOverride)
	{
		return MaterialSetOverride;
	}
	if (CachedMaterialSet)
	{
		return CachedMaterialSet;
	}
	return Cast<UForgeArmorMaterialSet>(UForgeArmorSettings::Get()->MaterialSet.TryLoad());
}

UForgeVehicleDamageComponent* UForgeVehicleArmorComponent::GetDamageComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UForgeVehicleDamageComponent>() : nullptr;
}

void UForgeVehicleArmorComponent::SpawnZones()
{
	if (!ArmorProfile || !GetOwner())
	{
		return;
	}

	USceneComponent* AttachTarget = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!AttachTarget)
	{
		AttachTarget = GetOwner()->GetRootComponent();
	}
	if (!AttachTarget)
	{
		UE_LOG(LogForgeArmor, Warning, TEXT("%s: no component to attach armor zones to."), *GetNameSafe(GetOwner()));
		return;
	}

	const ECollisionChannel ProjectileChannel = ECC_GameTraceChannel10;
	const bool bAuthority = GetOwner()->HasAuthority();

	for (const FForgeArmorZoneDef& Zone : ArmorProfile->Zones)
	{
		if (Zone.Binding == EForgeArmorZoneBinding::ChildBox)
		{
			UForgeArmorZoneComponent* ZoneComponent = NewObject<UForgeArmorZoneComponent>(GetOwner(), UForgeArmorZoneComponent::StaticClass(), *FString::Printf(TEXT("ArmorZone_%s"), *Zone.ZoneId.ToString()));
			ZoneComponent->ZoneId = Zone.ZoneId;
			ZoneComponent->MaterialPhysMatTag = Zone.MaterialTag;
			ZoneComponent->OwningArmor = this;
			ZoneComponent->SetBoxExtent(Zone.BoxExtentCm, false);
			ZoneComponent->ConfigureZoneCollision(ProjectileChannel);
			ZoneComponent->SetupAttachment(AttachTarget, Zone.AttachSocket);
			ZoneComponent->RegisterComponent();
			ZoneComponent->SetRelativeTransform(Zone.LocalTransform);
			SpawnedZones.Add(ZoneComponent);
		}
		else // ExistingComponent
		{
			TArray<UActorComponent*> Tagged = GetOwner()->GetComponentsByTag(UPrimitiveComponent::StaticClass(), Zone.ComponentTag);
			for (UActorComponent* Component : Tagged)
			{
				if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
				{
					AdoptedZoneComponents.Add(Primitive, Zone.ZoneId);
				}
			}
		}

		if (bAuthority)
		{
			FForgeArmorZoneState& State = ZoneStates.Items.AddDefaulted_GetRef();
			State.ZoneId = Zone.ZoneId;
			State.ERATilesRemaining = Zone.ERATileCount;
			ZoneStates.MarkItemDirty(State);
		}
	}
}

void UForgeVehicleArmorComponent::DestroyZones()
{
	for (UForgeArmorZoneComponent* Zone : SpawnedZones)
	{
		if (IsValid(Zone))
		{
			Zone->DestroyComponent();
		}
	}
	SpawnedZones.Reset();
	AdoptedZoneComponents.Reset();
}

void UForgeVehicleArmorComponent::SetArmorArmed(const bool bArmed)
{
	if (bArmorArmed == bArmed)
	{
		return;
	}
	bArmorArmed = bArmed;

	for (UForgeArmorZoneComponent* Zone : SpawnedZones)
	{
		if (IsValid(Zone))
		{
			Zone->SetArmed(bArmed);
		}
	}
	for (const TPair<TWeakObjectPtr<const UPrimitiveComponent>, FName>& Pair : AdoptedZoneComponents)
	{
		if (UPrimitiveComponent* Primitive = const_cast<UPrimitiveComponent*>(Pair.Key.Get()))
		{
			if (bArmed)
			{
				Primitive->ComponentTags.AddUnique(ForgeArmor::GetImpenetrableTagName());
			}
			else
			{
				Primitive->ComponentTags.Remove(ForgeArmor::GetImpenetrableTagName());
			}
		}
	}

	ApplyHullProjectileCollision(bArmed);
}

void UForgeVehicleArmorComponent::ApplyHullProjectileCollision(const bool bIgnoreProjectiles)
{
	const ECollisionChannel ProjectileChannel = ECC_GameTraceChannel10;

	if (bIgnoreProjectiles)
	{
		SuppressedHullComponents.Reset();
		TInlineComponentArray<UPrimitiveComponent*> Primitives(GetOwner());
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive || Primitive->IsA<UForgeArmorZoneComponent>())
			{
				continue;
			}
			if (AdoptedZoneComponents.Contains(Primitive))
			{
				continue;
			}
			if (Primitive->ComponentHasTag(KeepProjectileCollisionTag))
			{
				continue;
			}
			if (Primitive->GetCollisionResponseToChannel(ProjectileChannel) == ECR_Ignore)
			{
				continue;
			}
			Primitive->SetCollisionResponseToChannel(ProjectileChannel, ECR_Ignore);
			SuppressedHullComponents.Add(Primitive);
		}
	}
	else
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Suppressed : SuppressedHullComponents)
		{
			if (UPrimitiveComponent* Primitive = Suppressed.Get())
			{
				Primitive->SetCollisionResponseToChannel(ProjectileChannel, ECR_Block);
			}
		}
		SuppressedHullComponents.Reset();
	}
}

const FForgeArmorZoneDef* UForgeVehicleArmorComponent::ResolveZone(const UPrimitiveComponent* HitComponent) const
{
	if (!HitComponent || !ArmorProfile)
	{
		return nullptr;
	}
	if (const UForgeArmorZoneComponent* ZoneComponent = Cast<UForgeArmorZoneComponent>(HitComponent))
	{
		return ArmorProfile->FindZone(ZoneComponent->ZoneId);
	}
	if (const FName* ZoneId = AdoptedZoneComponents.Find(HitComponent))
	{
		return ArmorProfile->FindZone(*ZoneId);
	}
	return nullptr;
}

FForgeArmorZoneState* UForgeVehicleArmorComponent::FindZoneState(const FName& ZoneId)
{
	return ZoneStates.Items.FindByPredicate([&ZoneId](const FForgeArmorZoneState& State) { return State.ZoneId == ZoneId; });
}

const FForgeArmorZoneState* UForgeVehicleArmorComponent::FindZoneState(const FName& ZoneId) const
{
	return ZoneStates.Items.FindByPredicate([&ZoneId](const FForgeArmorZoneState& State) { return State.ZoneId == ZoneId; });
}

bool UForgeVehicleArmorComponent::ConsumeERATile(const FName& ZoneId)
{
	if (FForgeArmorZoneState* State = FindZoneState(ZoneId))
	{
		if (State->ERATilesRemaining > 0)
		{
			--State->ERATilesRemaining;
			ZoneStates.MarkItemDirty(*State);
			OnZoneStateChanged.Broadcast(*State);
			return true;
		}
	}
	return false;
}

void UForgeVehicleArmorComponent::CompromiseZone(const FName& ZoneId)
{
	if (FForgeArmorZoneState* State = FindZoneState(ZoneId))
	{
		if (!State->bCompromised)
		{
			State->bCompromised = true;
			ZoneStates.MarkItemDirty(*State);
			OnZoneStateChanged.Broadcast(*State);
		}
	}
}

void UForgeVehicleArmorComponent::HandleZoneStateReplicated(const FForgeArmorZoneState& ZoneState)
{
	OnZoneStateChanged.Broadcast(ZoneState);
}

void UForgeVehicleArmorComponent::GatherZoneStack(const FForgeArmorZoneDef& OuterZone, TArray<const FForgeArmorZoneDef*>& OutStack) const
{
	OutStack.Reset();
	const FForgeArmorZoneDef* Current = &OuterZone;
	int32 Guard = 0;
	while (Current && Guard++ < 4)
	{
		OutStack.Add(Current);
		Current = Current->BackingZoneId.IsNone() ? nullptr : ArmorProfile->FindZone(Current->BackingZoneId);
	}
}

double UForgeVehicleArmorComponent::GetHESHFactor(const UPrimitiveComponent* HitComponent) const
{
	const FForgeArmorZoneDef* Zone = ResolveZone(HitComponent);
	const UForgeArmorMaterialSet* Materials = GetMaterialSet();
	if (!Zone || !Materials)
	{
		return 1.0;
	}
	if (const FForgeArmorMaterialSpec* Material = Materials->FindMaterialPtr(Zone->MaterialTag))
	{
		return Material->HESHFactor;
	}
	return 1.0;
}

bool UForgeVehicleArmorComponent::BuildCEPath(const UPrimitiveComponent* HitComponent, const double AngleFromNormalDeg, TArray<ForgeArmor::Ballistics::FCEPathElement>& OutPath, double& OutSpallSuppression, FName& OutZoneId) const
{
	using namespace ForgeArmor::Ballistics;

	OutPath.Reset();
	OutSpallSuppression = 0.0;

	const FForgeArmorZoneDef* OuterZone = ResolveZone(HitComponent);
	const UForgeArmorMaterialSet* Materials = GetMaterialSet();
	if (!OuterZone || !Materials)
	{
		return false;
	}
	OutZoneId = OuterZone->ZoneId;

	TArray<const FForgeArmorZoneDef*> Stack;
	GatherZoneStack(*OuterZone, Stack);

	for (const FForgeArmorZoneDef* Zone : Stack)
	{
		const FForgeArmorMaterialSpec* Material = Materials->FindMaterialPtr(Zone->MaterialTag);
		const FForgeArmorZoneState* State = FindZoneState(Zone->ZoneId);

		FCEPathElement Element;
		Element.ThicknessMM = LineOfSightThickness(Zone->ThicknessMM, AngleFromNormalDeg);
		Element.KFactorCE = Material ? Material->RHAeVsCE : 1.0;
		if (Material && Material->bIsERA && State && State->ERATilesRemaining > 0)
		{
			Element.bIntactERA = true;
			Element.ERACutCEMM = Material->ERACutCEMM;
		}
		if (Material)
		{
			OutSpallSuppression = FMath::Max(OutSpallSuppression, (double)Material->SpallSuppression);
		}
		OutPath.Add(Element);
	}
	return true;
}

FForgeArmorInteraction UForgeVehicleArmorComponent::HandleKineticImpact(
	const FHitResult& HitResult,
	const FVector& ImpactVelocityCmS,
	const FForgeAmmoBallisticSpec& AmmoSpec,
	const double ProjectileMassKG,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	using namespace ForgeArmor::Ballistics;

	FForgeArmorInteraction Interaction;

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return Interaction;
	}

	const FForgeArmorZoneDef* OuterZone = ResolveZone(HitResult.GetComponent());
	const UForgeArmorMaterialSet* Materials = GetMaterialSet();
	if (!Materials)
	{
		return Interaction;
	}

	// Fall back to the profile's catch-all skin for unmapped adopted primitives.
	FForgeArmorZoneDef FallbackZone;
	if (!OuterZone)
	{
		FallbackZone.ZoneId = TEXT("Default");
		FallbackZone.MaterialTag = ArmorProfile ? ArmorProfile->DefaultMaterialTag : FGameplayTag();
		FallbackZone.ThicknessMM = ArmorProfile ? ArmorProfile->DefaultThicknessMM : 8.f;
		OuterZone = &FallbackZone;
	}
	Interaction.ZoneId = OuterZone->ZoneId;

	const FVector Direction = ImpactVelocityCmS.GetSafeNormal();
	const double ImpactSpeedMS = ImpactVelocityCmS.Size() / 100.0;
	const FVector PlateNormal = HitResult.ImpactNormal.GetSafeNormal();
	const double CosAngle = FMath::Clamp(FMath::Abs(FVector::DotProduct(Direction, PlateNormal)), 0.0001, 1.0);
	const double AngleFromNormalDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));

	TArray<const FForgeArmorZoneDef*> Stack;
	GatherZoneStack(*OuterZone, Stack);

	// Walk the stack: ERA reduces the available penetration, solid plates stack Teff.
	double PenAvailableMM = PenetrationAtSpeed(AmmoSpec.PenetratorClass, AmmoSpec.PenetrationRefMM, AmmoSpec.ReferenceSpeedMS, ImpactSpeedMS, AmmoSpec.DeMarreExponentOverride);
	double TotalEffectiveMM = 0.0;
	double TotalLOSMM = 0.0;
	double SpallSuppression = 0.0;
	const FForgeArmorZoneDef* OutermostSolidPlate = nullptr;

	for (const FForgeArmorZoneDef* Zone : Stack)
	{
		const FForgeArmorMaterialSpec* Material = Materials->FindMaterialPtr(Zone->MaterialTag);
		const FForgeArmorZoneState* State = FindZoneState(Zone->ZoneId);

		if (Material && Material->bIsERA)
		{
			if (State && State->ERATilesRemaining > 0)
			{
				const double LongRodFactor = AmmoSpec.PenetratorClass == EForgePenetratorClass::APFSDS ? Material->ERALongRodFactor : 1.0;
				PenAvailableMM = FMath::Max(PenAvailableMM - Material->ERACutKEMM * LongRodFactor, 0.0);
				ConsumeERATile(Zone->ZoneId);
			}
			// Spent tiles still contribute their (thin) carcass plate below.
		}

		FPlate Plate;
		Plate.ThicknessMM = Zone->ThicknessMM;
		Plate.KFactorKE = Material ? Material->RHAeVsKE : 1.0;
		if (State && State->bCompromised)
		{
			Plate.KFactorKE *= 0.85;
		}
		TotalEffectiveMM += EffectiveThickness(Plate, AmmoSpec.PenetratorClass, AngleFromNormalDeg, AmmoSpec.CaliberMM);
		TotalLOSMM += LineOfSightThickness(Zone->ThicknessMM, AngleFromNormalDeg);
		if (Material)
		{
			SpallSuppression = FMath::Max(SpallSuppression, (double)Material->SpallSuppression);
		}
		if (!OutermostSolidPlate && (!Material || !Material->bIsERA))
		{
			OutermostSolidPlate = Zone;
		}
	}

	Interaction.PenAvailableMM = PenAvailableMM;
	Interaction.EffectiveThicknessMM = TotalEffectiveMM;

	const double ImpactEnergyJ = KineticEnergyJ(ProjectileMassKG, ImpactSpeedMS);
	UForgeVehicleDamageComponent* Damage = GetDamageComponent();

	FForgeArmorImpactFXEvent FXEvent;
	FXEvent.Location = HitResult.ImpactPoint;
	FXEvent.Normal = PlateNormal;
	FXEvent.ZoneId = Interaction.ZoneId;
	FXEvent.CaliberClass = QuantizeCaliberClass(AmmoSpec.CaliberMM);
	if (const FForgeArmorMaterialSpec* OuterMaterial = Materials->FindMaterialPtr(OuterZone->MaterialTag))
	{
		FXEvent.SurfaceType = OuterMaterial->SurfaceType;
	}

	// ---- Ricochet check on the outermost solid plate ------------------------
	if (OutermostSolidPlate && AmmoSpec.IsKinetic())
	{
		const double CriticalAngle = RicochetCriticalAngleDeg(AmmoSpec.PenetratorClass, AmmoSpec.CaliberMM, OutermostSolidPlate->ThicknessMM);
		const double Probability = RicochetProbability(AngleFromNormalDeg, CriticalAngle);
		if (Probability > 0.0 && FMath::FRand() <= Probability)
		{
			const double ResidualSpeedMS = ImpactSpeedMS * RicochetSpeedRetention(AngleFromNormalDeg, CriticalAngle);
			const FVector Reflected = Direction - 2.0 * FVector::DotProduct(Direction, PlateNormal) * PlateNormal;

			Interaction.Outcome = EForgePenetrationOutcome::Ricochet;
			Interaction.bShouldContinue = ResidualSpeedMS >= MinRicochetSpeedMS;
			Interaction.ContinuationLocation = HitResult.ImpactPoint + Reflected * 5.0;
			Interaction.ContinuationVelocityCmS = Reflected * ResidualSpeedMS * 100.0;

			FXEvent.Outcome = EForgePenetrationOutcome::Ricochet;
			if (Damage)
			{
				Damage->BroadcastImpactFX(FXEvent);
			}
			DebugDrawInteraction(HitResult, Interaction, AngleFromNormalDeg);
			return Interaction;
		}
	}

	// ---- Penetration solve ---------------------------------------------------
	if (PenAvailableMM > TotalEffectiveMM && TotalEffectiveMM >= 0.0)
	{
		const double Exponent = AmmoSpec.DeMarreExponentOverride > 0.f ? AmmoSpec.DeMarreExponentOverride : DeMarreExponent(AmmoSpec.PenetratorClass);
		const double ResidualFraction = (PenAvailableMM - TotalEffectiveMM) / FMath::Max((double)AmmoSpec.PenetrationRefMM, 1.0);
		const double ResidualSpeedMS = FMath::Min((double)AmmoSpec.ReferenceSpeedMS * FMath::Pow(ResidualFraction, 1.0 / Exponent), ImpactSpeedMS);
		const double EnergyImpartedJ = ImpactEnergyJ - KineticEnergyJ(ProjectileMassKG, ResidualSpeedMS);

		Interaction.Outcome = EForgePenetrationOutcome::Penetrated;

		// Continue the round on the far side of the (authored) plate stack.
		const double PlateDepthCm = TotalLOSMM / 10.0 + 5.0;
		const FVector ExitPoint = HitResult.ImpactPoint + Direction * PlateDepthCm;

		// Small exit yaw: perforating rounds wobble.
		const FVector ExitDirection = Direction.RotateAngleAxis(FMath::FRandRange(0.5f, 2.0f), FVector::CrossProduct(Direction, PlateNormal).GetSafeNormal());

		Interaction.bShouldContinue = ResidualSpeedMS >= MinContinuationSpeedMS;
		Interaction.ContinuationLocation = ExitPoint;
		Interaction.ContinuationVelocityCmS = ExitDirection * ResidualSpeedMS * 100.0;

		if (Damage)
		{
			const FSpallCone Spall = ComputeSpall(TotalEffectiveMM, PenAvailableMM, TotalLOSMM, EnergyImpartedJ, SpallSuppression);

			FForgeInteriorEvent Interior;
			Interior.OriginWS = ExitPoint;
			Interior.DirectionWS = Direction;
			Interior.ConeHalfAngleDeg = Spall.HalfAngleDeg;
			Interior.FragmentCount = Spall.FragmentCount;
			Interior.TotalEnergyJ = Spall.TotalEnergyJ;
			Interior.bIncendiary = AmmoSpec.bIncendiary;
			Interior.InstigatorController = InstigatorController;
			Interior.DamageCauser = DamageCauser;
			Interior.SourceZoneId = Interaction.ZoneId;
			Damage->ProcessBehindArmor(Interior);

			// APHE: armed by a plate at/above the fuze threshold, bursts inside.
			if (AmmoSpec.bHasBurstingCharge && TotalLOSMM >= AmmoSpec.FuzeArmThresholdMM)
			{
				const FVector BurstPoint = HitResult.ImpactPoint + Direction * (AmmoSpec.FuzeTravelM * 100.0);
				Damage->ProcessAPHEBurst(BurstPoint, AmmoSpec.BurstChargeGrams, InstigatorController, DamageCauser);
				Interaction.bShouldContinue = false; // the shell is spent in the burst
			}

			FXEvent.Outcome = EForgePenetrationOutcome::Penetrated;
			Damage->BroadcastImpactFX(FXEvent);
		}
	}
	else
	{
		Interaction.Outcome = EForgePenetrationOutcome::Stopped;
		const double PartialFraction = TotalEffectiveMM > 0.0 ? FMath::Clamp(PenAvailableMM / TotalEffectiveMM, 0.0, 1.0) : 0.0;

		if (Damage)
		{
			// Near-perforation: back-face spall sheds into the interior at reduced energy.
			if (PartialFraction >= 0.85)
			{
				const FSpallCone Spall = ComputeSpall(TotalEffectiveMM, FMath::Max(PenAvailableMM, 1.0), TotalLOSMM, ImpactEnergyJ * 0.3, SpallSuppression);
				FForgeInteriorEvent Interior;
				Interior.OriginWS = HitResult.ImpactPoint + Direction * (TotalLOSMM / 10.0);
				Interior.DirectionWS = Direction;
				Interior.ConeHalfAngleDeg = FMath::Max(Spall.HalfAngleDeg, 30.0);
				Interior.FragmentCount = FMath::Max(Spall.FragmentCount / 2, 2);
				Interior.TotalEnergyJ = Spall.TotalEnergyJ;
				Interior.bIncendiary = false;
				Interior.InstigatorController = InstigatorController;
				Interior.DamageCauser = DamageCauser;
				Interior.SourceZoneId = Interaction.ZoneId;
				Damage->ProcessBehindArmor(Interior);

				CompromiseZone(OutermostSolidPlate ? OutermostSolidPlate->ZoneId : Interaction.ZoneId);
			}

			// Non-penetrating hits still rattle externally mounted modules nearby.
			Damage->ProcessExternalHit(HitResult.ImpactPoint, ImpactEnergyJ * 0.05, 30.0 + AmmoSpec.CaliberMM * 2.0, InstigatorController, DamageCauser);

			FXEvent.Outcome = EForgePenetrationOutcome::Stopped;
			Damage->BroadcastImpactFX(FXEvent);
		}
	}

	DebugDrawInteraction(HitResult, Interaction, AngleFromNormalDeg);
	return Interaction;
}

void UForgeVehicleArmorComponent::DebugDrawInteraction(const FHitResult& HitResult, const FForgeArmorInteraction& Interaction, const double AngleFromNormalDeg) const
{
#if ENABLE_DRAW_DEBUG
	if (!ShouldDebugDraw() || !GetWorld())
	{
		return;
	}
	const FColor Color = Interaction.Outcome == EForgePenetrationOutcome::Penetrated ? FColor::Red
		: Interaction.Outcome == EForgePenetrationOutcome::Ricochet ? FColor::Yellow : FColor::Green;
	DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 12.f, Color, false, 10.f);
	if (Interaction.bShouldContinue)
	{
		DrawDebugLine(GetWorld(), Interaction.ContinuationLocation, Interaction.ContinuationLocation + Interaction.ContinuationVelocityCmS.GetSafeNormal() * 100.f, Color, false, 10.f, 0, 1.5f);
	}
	DrawDebugString(GetWorld(), HitResult.ImpactPoint + FVector(0, 0, 20),
		FString::Printf(TEXT("%s: pen %.0fmm vs %.0fmm @%.0fdeg"), *Interaction.ZoneId.ToString(), Interaction.PenAvailableMM, Interaction.EffectiveThicknessMM, AngleFromNormalDeg),
		nullptr, Color, 10.f);
#endif
}
