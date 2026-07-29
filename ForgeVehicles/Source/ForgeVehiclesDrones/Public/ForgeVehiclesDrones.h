// Copyright Impact-Forge. ForgeVehiclesDrones module interface.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

FORGEVEHICLESDRONES_API DECLARE_LOG_CATEGORY_EXTERN(LogForgeDrones, Log, All);

class FForgeVehiclesDronesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
