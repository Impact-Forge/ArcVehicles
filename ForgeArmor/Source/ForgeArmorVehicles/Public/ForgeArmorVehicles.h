// Copyright Impact-Forge. ForgeArmorVehicles module interface.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FForgeArmorVehiclesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
