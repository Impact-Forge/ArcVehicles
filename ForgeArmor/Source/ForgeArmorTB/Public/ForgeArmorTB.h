// Copyright Impact-Forge. ForgeArmorTB module interface.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FForgeArmorTBModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
