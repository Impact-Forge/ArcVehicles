// Copyright Impact-Forge. ForgeArmorCore module interface.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

FORGEARMORCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogForgeArmor, Log, All);

class FForgeArmorCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
