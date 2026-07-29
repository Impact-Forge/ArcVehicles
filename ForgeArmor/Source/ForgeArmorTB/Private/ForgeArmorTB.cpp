// Copyright Impact-Forge. ForgeArmorTB module implementation.

#include "ForgeArmorTB.h"

#include "Components/ForgeArmorZoneComponent.h"
#include "Misc/TBTags.h"
#include "Modules/ModuleManager.h"

void FForgeArmorTBModule::StartupModule()
{
	// Keep the core's impenetrable component tag in lock-step with the Terminal
	// Ballistics API: if a TB update renames the tag, zones follow automatically.
	ForgeArmor::SetImpenetrableTagName(TB::Tags::PlainTag_IMPENETRABLE);
}

void FForgeArmorTBModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FForgeArmorTBModule, ForgeArmorTB)
