// Copyright Impact-Forge. Ammo performance map implementation.

#include "ForgeAmmoPerformanceMap.h"

#include "Ballistics/ForgeArmorBallistics.h"
#include "Core/TBBulletDataAsset.h"
#include "Types/TBBulletInfo.h"
#include "Types/TBBulletPhysicalProperties.h"

FForgeAmmoBallisticSpec UForgeAmmoPerformanceMap::Resolve(const UBulletDataAsset* BulletAsset, const FTBBulletInfo& BulletInfo, const FTBBulletPhysicalProperties& Properties) const
{
	if (BulletAsset)
	{
		for (const TPair<TSoftObjectPtr<UBulletDataAsset>, FForgeAmmoBallisticSpec>& Pair : ByBulletAsset)
		{
			if (Pair.Key.Get() == BulletAsset)
			{
				return Pair.Value;
			}
		}
	}
	if (!BulletInfo.BulletName.IsNone())
	{
		if (const FForgeAmmoBallisticSpec* Found = ByBulletName.Find(BulletInfo.BulletName))
		{
			return *Found;
		}
	}
	return EstimateFromProperties(Properties);
}

FForgeAmmoBallisticSpec UForgeAmmoPerformanceMap::EstimateFromProperties(const FTBBulletPhysicalProperties& Properties)
{
	FForgeAmmoBallisticSpec Spec;
	Spec.PenetratorClass = EForgePenetratorClass::FMJ; // unmapped ammo is assumed to be ball
	Spec.CaliberMM = (float)FMath::Max(Properties.Radius * 20.0, 1.0); // cm radius -> mm diameter
	Spec.ReferenceSpeedMS = 850.f;
	Spec.PenetrationRefMM = (float)ForgeArmor::Ballistics::EstimatePenetrationRefMM(
		Spec.PenetratorClass, Properties.Mass, Spec.CaliberMM, Spec.ReferenceSpeedMS);
	return Spec;
}
