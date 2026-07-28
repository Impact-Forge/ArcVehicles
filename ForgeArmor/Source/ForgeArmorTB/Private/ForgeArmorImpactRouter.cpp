// Copyright Impact-Forge. Terminal Ballistics impact routing implementation.

#include "ForgeArmorImpactRouter.h"

#include "Components/ForgeArmorZoneComponent.h"
#include "Components/ForgeVehicleArmorComponent.h"
#include "Components/ForgeVehicleDamageComponent.h"
#include "Core/TBBulletDataAsset.h"
#include "Engine/World.h"
#include "ForgeAmmoPerformanceMap.h"
#include "ForgeArmorCore.h"
#include "ForgeArmorSettings.h"
#include "GameFramework/Pawn.h"
#include "Subsystems/ForgeArmorRegistrySubsystem.h"
#include "Subsystems/TerminalBallisticsSubsystem.h"
#include "Types/TBLaunchTypes.h"
#include "Types/TBSimData.h"

int32 UForgeArmorChainSubsystem::GetChainDepth(const FTBProjectileId& Id) const
{
	if (const int32* Depth = ChainDepths.Find(Id.Guid))
	{
		return *Depth;
	}
	return 0;
}

void UForgeArmorChainSubsystem::RecordChild(const FTBProjectileId& Child, const int32 Depth)
{
	// Ids are transient; keep the map bounded instead of tracking completions.
	if (ChainDepths.Num() > 1024)
	{
		ChainDepths.Reset();
	}
	ChainDepths.Add(Child.Guid, Depth);
}

namespace
{
	AController* DeriveInstigatorController(const FTBImpactParams& ImpactParams)
	{
		if (const APawn* InstigatingPawn = Cast<APawn>(ImpactParams.InstigatingActor))
		{
			return InstigatingPawn->GetController();
		}
		return nullptr;
	}

	FForgeAmmoBallisticSpec ResolveAmmoSpec(const FTBImpactParams& ImpactParams, const UBulletDataAsset* BulletAsset)
	{
		if (const UForgeAmmoPerformanceMap* AmmoMap = Cast<UForgeAmmoPerformanceMap>(UForgeArmorSettings::Get()->AmmoPerformanceMap.TryLoad()))
		{
			return AmmoMap->Resolve(BulletAsset, ImpactParams.BulletInfo, ImpactParams.BulletProperties);
		}
		return UForgeAmmoPerformanceMap::EstimateFromProperties(ImpactParams.BulletProperties);
	}
}

bool UForgeArmorImpactRouter::RouteBulletImpact(const UObject* WorldContextObject, const FTBImpactParams& ImpactParams)
{
	const UForgeArmorSettings* Settings = UForgeArmorSettings::Get();
	if (!Settings->bArmorSystemEnabled || !WorldContextObject)
	{
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	UForgeArmorRegistrySubsystem* Registry = World->GetSubsystem<UForgeArmorRegistrySubsystem>();
	if (!Registry)
	{
		return false;
	}

	AActor* HitActor = ImpactParams.HitResult.GetActor();
	UForgeVehicleArmorComponent* Armor = Registry->FindArmorForActor(HitActor);
	if (!Armor)
	{
		return false;
	}

	// Only impacts on armor zones are ours; hits on components that kept their
	// projectile collision (tires, external stores) flow through the injure path.
	const UPrimitiveComponent* HitComponent = ImpactParams.HitResult.GetComponent();
	const bool bIsZoneHit = Cast<UForgeArmorZoneComponent>(HitComponent) != nullptr || Armor->ResolveZone(HitComponent) != nullptr;
	if (!bIsZoneHit)
	{
		return false;
	}

	const UBulletDataAsset* BulletAsset = Cast<UBulletDataAsset>(ImpactParams.Payload);
	const FForgeAmmoBallisticSpec AmmoSpec = ResolveAmmoSpec(ImpactParams, BulletAsset);
	AController* InstigatorController = DeriveInstigatorController(ImpactParams);

	const FForgeArmorInteraction Interaction = Armor->HandleKineticImpact(
		ImpactParams.HitResult,
		ImpactParams.ImpactVelocity,
		AmmoSpec,
		ImpactParams.BulletProperties.Mass,
		InstigatorController,
		ImpactParams.InstigatingActor);

	// Continue perforating / deflected rounds as fresh ballistic projectiles.
	if (Interaction.bShouldContinue)
	{
		UForgeArmorChainSubsystem* Chains = World->GetSubsystem<UForgeArmorChainSubsystem>();
		const int32 Depth = Chains ? Chains->GetChainDepth(ImpactParams.ProjectileId) : 0;

		if (Depth >= Settings->MaxShotChainDepth)
		{
			UE_LOG(LogForgeArmor, Verbose, TEXT("Shot chain depth cap reached (%d), dropping continuation."), Depth);
		}
		else if (!BulletAsset)
		{
			// Without a bullet data asset in the payload we cannot rebuild the TB
			// projectile. Interior damage is already applied; only the residual
			// flight is lost. Fire wiring should pass the data asset as Payload.
			UE_LOG(LogForgeArmor, Log, TEXT("Armor continuation dropped: launch payload carries no UBulletDataAsset (bullet %s)."), *ImpactParams.BulletInfo.BulletName.ToString());
		}
		else if (UTerminalBallisticsSubsystem* Ballistics = World->GetSubsystem<UTerminalBallisticsSubsystem>())
		{
			FTBBulletSimData SimData(BulletAsset);
			SimData.Payload = ImpactParams.Payload;

			const FVector Direction = Interaction.ContinuationVelocityCmS.GetSafeNormal();
			FTBLaunchParams LaunchParams;
			LaunchParams.ProjectileSpeed = Interaction.ContinuationVelocityCmS.Size() / 100.0; // cm/s -> m/s
			LaunchParams.EffectiveRange = 400.0;
			LaunchParams.FireTransform = FTransform(Direction.Rotation(), Interaction.ContinuationLocation);
			LaunchParams.Owner = ImpactParams.InstigatingActor;
			LaunchParams.Instigator = InstigatorController;
			LaunchParams.AddToOwnerVelocity = false;
			LaunchParams.bForceNoTracer = true;
			LaunchParams.OwnerIgnoreDistance = 0.0;
			LaunchParams.Payload = ImpactParams.Payload;

			const FTBProjectileId ChildId = Ballistics->AddAndFireBullet(SimData, LaunchParams);
			if (Chains)
			{
				Chains->RecordChild(ChildId, Depth + 1);
			}
		}
	}

	return true;
}

bool UForgeArmorImpactRouter::RouteBulletInjure(const UObject* WorldContextObject, const FTBImpactParams& ImpactParams, const FTBProjectileInjuryParams& InjuryParams, const bool bExited, const FHitResult& ExitHit)
{
	const UForgeArmorSettings* Settings = UForgeArmorSettings::Get();
	if (!Settings->bArmorSystemEnabled || !WorldContextObject)
	{
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	UForgeArmorRegistrySubsystem* Registry = World->GetSubsystem<UForgeArmorRegistrySubsystem>();
	UForgeVehicleArmorComponent* Armor = Registry ? Registry->FindArmorForActor(ImpactParams.HitResult.GetActor()) : nullptr;
	if (!Armor)
	{
		return false;
	}

	// A penetrating hit on something that kept projectile collision: tires,
	// externally mounted optics, stowage. Damage nearby external modules.
	if (UForgeVehicleDamageComponent* Damage = Armor->GetDamageComponent())
	{
		Damage->ProcessExternalHit(
			InjuryParams.ImpactLocation,
			InjuryParams.ImpartedEnergy,
			/*RadiusCm*/ 60.0,
			DeriveInstigatorController(ImpactParams),
			ImpactParams.InstigatingActor);
	}
	return true;
}
