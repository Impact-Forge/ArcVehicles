// Copyright Impact-Forge. Armor profile implementation.

#include "Data/ForgeArmorProfile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FForgeArmorZoneDef* UForgeArmorProfile::FindZone(const FName& ZoneId) const
{
	return Zones.FindByPredicate([&ZoneId](const FForgeArmorZoneDef& Zone)
	{
		return Zone.ZoneId == ZoneId;
	});
}

#if WITH_EDITOR
EDataValidationResult UForgeArmorProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TSet<FName> Seen;
	for (const FForgeArmorZoneDef& Zone : Zones)
	{
		if (Zone.ZoneId.IsNone())
		{
			Context.AddError(NSLOCTEXT("ForgeArmor", "ZoneIdMissing", "Armor zone has no ZoneId."));
			Result = EDataValidationResult::Invalid;
		}
		else if (Seen.Contains(Zone.ZoneId))
		{
			Context.AddError(FText::Format(NSLOCTEXT("ForgeArmor", "ZoneIdDuplicate", "Duplicate armor zone id {0}."), FText::FromName(Zone.ZoneId)));
			Result = EDataValidationResult::Invalid;
		}
		Seen.Add(Zone.ZoneId);

		if (!Zone.MaterialTag.IsValid())
		{
			Context.AddError(FText::Format(NSLOCTEXT("ForgeArmor", "ZoneMaterialMissing", "Armor zone {0} has no material tag."), FText::FromName(Zone.ZoneId)));
			Result = EDataValidationResult::Invalid;
		}
		if (Zone.ThicknessMM < 1.f)
		{
			Context.AddError(FText::Format(NSLOCTEXT("ForgeArmor", "ZoneTooThin", "Armor zone {0} thickness must be >= 1mm."), FText::FromName(Zone.ZoneId)));
			Result = EDataValidationResult::Invalid;
		}
		if (!Zone.BackingZoneId.IsNone() && !FindZone(Zone.BackingZoneId))
		{
			Context.AddError(FText::Format(NSLOCTEXT("ForgeArmor", "ZoneBackingMissing", "Armor zone {0} references missing backing zone {1}."), FText::FromName(Zone.ZoneId), FText::FromName(Zone.BackingZoneId)));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif
