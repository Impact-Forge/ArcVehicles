// Copyright Impact-Forge. Armor registry + warhead evaluators implementation.

#include "Subsystems/ForgeArmorRegistrySubsystem.h"

#include "Ballistics/ForgeArmorBallistics.h"
#include "Components/ForgeVehicleArmorComponent.h"
#include "Components/ForgeVehicleDamageComponent.h"
#include "Data/ForgeWarheadSpec.h"
#include "Engine/World.h"
#include "ForgeArmorCore.h"
#include "GameFramework/Actor.h"

UForgeArmorRegistrySubsystem* UForgeArmorRegistrySubsystem::Get(const UObject* WorldContextObject)
{
	if (WorldContextObject)
	{
		if (const UWorld* World = WorldContextObject->GetWorld())
		{
			return World->GetSubsystem<UForgeArmorRegistrySubsystem>();
		}
	}
	return nullptr;
}

void UForgeArmorRegistrySubsystem::RegisterArmor(UForgeVehicleArmorComponent* ArmorComponent)
{
	if (ArmorComponent)
	{
		RegisteredArmor.AddUnique(ArmorComponent);
	}
}

void UForgeArmorRegistrySubsystem::UnregisterArmor(UForgeVehicleArmorComponent* ArmorComponent)
{
	RegisteredArmor.Remove(ArmorComponent);
}

UForgeVehicleArmorComponent* UForgeArmorRegistrySubsystem::FindArmorForActor(const AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	for (UForgeVehicleArmorComponent* Armor : RegisteredArmor)
	{
		if (Armor && Armor->GetOwner() == Actor)
		{
			return Armor;
		}
	}
	return nullptr;
}

bool UForgeArmorRegistrySubsystem::EvaluateCEImpact(const FHitResult& HitResult, const UForgeWarheadSpec* Warhead, const float StandoffMM, AController* InstigatorController, AActor* DamageCauser, FForgeBehindArmorResult& OutResult)
{
	using namespace ForgeArmor::Ballistics;

	OutResult = FForgeBehindArmorResult();
	if (!Warhead)
	{
		return false;
	}

	UForgeVehicleArmorComponent* Armor = FindArmorForActor(HitResult.GetActor());
	if (!Armor)
	{
		return false;
	}

	// Grazing jets fail to fuze: >80 deg from the plate normal.
	const FVector Direction = (HitResult.TraceEnd - HitResult.TraceStart).GetSafeNormal();
	const double CosAngle = FMath::Clamp(FMath::Abs(FVector::DotProduct(Direction, HitResult.ImpactNormal.GetSafeNormal())), 0.0001, 1.0);
	const double AngleFromNormalDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));
	if (AngleFromNormalDeg > 80.0)
	{
		OutResult.Outcome = EForgePenetrationOutcome::Ricochet; // fuze failure, cosmetic only
		return true;
	}

	TArray<FCEPathElement> Path;
	double SpallSuppression = 0.0;
	FName ZoneId;
	if (!Armor->BuildCEPath(HitResult.GetComponent(), AngleFromNormalDeg, Path, SpallSuppression, ZoneId))
	{
		return false;
	}

	bool bERAConsumed = false;
	const double Residual = SolveCEResidualMM(Warhead->JetPenetrationMM, Path, StandoffMM, bERAConsumed);
	if (bERAConsumed)
	{
		Armor->ConsumeERATile(ZoneId);
	}

	OutResult.ResidualPenetrationMM = (float)Residual;

	UForgeVehicleDamageComponent* Damage = Armor->GetDamageComponent();

	if (Residual > 0.0)
	{
		OutResult.Outcome = EForgePenetrationOutcome::Penetrated;
		const FSpallCone Jet = ComputeJetCone(Residual, Warhead->JetPenetrationMM);
		OutResult.InteriorEnergyJ = (float)(Jet.TotalEnergyJ * (1.0 - SpallSuppression * 0.5));

		if (Damage)
		{
			FForgeInteriorEvent Interior;
			Interior.OriginWS = HitResult.ImpactPoint + Direction * 10.0;
			Interior.DirectionWS = Direction;
			Interior.ConeHalfAngleDeg = Jet.HalfAngleDeg;
			Interior.FragmentCount = Jet.FragmentCount;
			Interior.TotalEnergyJ = OutResult.InteriorEnergyJ;
			Interior.bIncendiary = true; // jets ignite what they touch
			Interior.InstigatorController = InstigatorController;
			Interior.DamageCauser = DamageCauser;
			Interior.SourceZoneId = ZoneId;
			Damage->ProcessBehindArmor(Interior);
		}
	}
	else
	{
		OutResult.Outcome = EForgePenetrationOutcome::Stopped;
	}

	if (Damage)
	{
		FForgeArmorImpactFXEvent FXEvent;
		FXEvent.Location = HitResult.ImpactPoint;
		FXEvent.Normal = HitResult.ImpactNormal;
		FXEvent.Outcome = OutResult.Outcome;
		FXEvent.ZoneId = ZoneId;
		FXEvent.CaliberClass = 4;
		Damage->BroadcastImpactFX(FXEvent);
	}
	return true;
}

bool UForgeArmorRegistrySubsystem::EvaluateHESHImpact(const FHitResult& HitResult, const UForgeWarheadSpec* Warhead, AController* InstigatorController, AActor* DamageCauser, FForgeBehindArmorResult& OutResult)
{
	using namespace ForgeArmor::Ballistics;

	OutResult = FForgeBehindArmorResult();
	if (!Warhead)
	{
		return false;
	}

	UForgeVehicleArmorComponent* Armor = FindArmorForActor(HitResult.GetActor());
	if (!Armor)
	{
		return false;
	}

	const FForgeArmorZoneDef* Zone = Armor->ResolveZone(HitResult.GetComponent());
	const double HESHFactor = Armor->GetHESHFactor(HitResult.GetComponent());
	const double PlateThickness = Zone ? Zone->ThicknessMM : 10.0;

	UForgeVehicleDamageComponent* Damage = Armor->GetDamageComponent();

	if (HESHProducesScab(PlateThickness, Warhead->CaliberMM, HESHFactor))
	{
		OutResult.Outcome = EForgePenetrationOutcome::Penetrated;
		const FSpallCone Scab = ComputeHESHScab(Warhead->CaliberMM, PlateThickness);
		OutResult.InteriorEnergyJ = (float)Scab.TotalEnergyJ;

		if (Damage)
		{
			const FVector Direction = (HitResult.TraceEnd - HitResult.TraceStart).GetSafeNormal();
			FForgeInteriorEvent Interior;
			Interior.OriginWS = HitResult.ImpactPoint + Direction * (PlateThickness / 10.0 + 5.0);
			Interior.DirectionWS = -HitResult.ImpactNormal;
			Interior.ConeHalfAngleDeg = Scab.HalfAngleDeg;
			Interior.FragmentCount = Scab.FragmentCount;
			Interior.TotalEnergyJ = Scab.TotalEnergyJ;
			Interior.InstigatorController = InstigatorController;
			Interior.DamageCauser = DamageCauser;
			if (Zone)
			{
				Interior.SourceZoneId = Zone->ZoneId;
			}
			Damage->ProcessBehindArmor(Interior);
		}
	}
	else
	{
		OutResult.Outcome = EForgePenetrationOutcome::Stopped;
	}

	// The blast component always batters external fittings.
	if (Damage)
	{
		Damage->ApplyBlast(HitResult.ImpactPoint, Warhead->TNTEquivalentKG, Warhead->BlastRadiusCm, InstigatorController, DamageCauser);

		FForgeArmorImpactFXEvent FXEvent;
		FXEvent.Location = HitResult.ImpactPoint;
		FXEvent.Normal = HitResult.ImpactNormal;
		FXEvent.Outcome = OutResult.Outcome;
		if (Zone)
		{
			FXEvent.ZoneId = Zone->ZoneId;
		}
		FXEvent.CaliberClass = 4;
		Damage->BroadcastImpactFX(FXEvent);
	}
	return true;
}

void UForgeArmorRegistrySubsystem::EvaluateBlast(const FVector& Origin, const UForgeWarheadSpec* Warhead, AController* InstigatorController, AActor* DamageCauser)
{
	if (!Warhead)
	{
		return;
	}
	for (UForgeVehicleArmorComponent* Armor : RegisteredArmor)
	{
		if (!Armor || !Armor->GetOwner())
		{
			continue;
		}
		const double Distance = FVector::Dist(Armor->GetOwner()->GetActorLocation(), Origin);
		if (Distance > Warhead->BlastRadiusCm)
		{
			continue;
		}
		if (UForgeVehicleDamageComponent* Damage = Armor->GetDamageComponent())
		{
			Damage->ApplyBlast(Origin, Warhead->TNTEquivalentKG, Warhead->BlastRadiusCm, InstigatorController, DamageCauser);
		}
	}
}
