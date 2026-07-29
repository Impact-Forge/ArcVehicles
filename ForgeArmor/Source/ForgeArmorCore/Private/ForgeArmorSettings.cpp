// Copyright Impact-Forge. Forge Armor settings implementation.

#include "ForgeArmorSettings.h"

UForgeArmorSettings::UForgeArmorSettings()
{
	CategoryName = TEXT("Plugins");
}

const UForgeArmorSettings* UForgeArmorSettings::Get()
{
	return GetDefault<UForgeArmorSettings>();
}
