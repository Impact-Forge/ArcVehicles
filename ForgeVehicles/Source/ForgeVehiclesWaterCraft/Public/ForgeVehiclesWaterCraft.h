// Copyright Impact-Forge. Water-craft physics authored for Forge Vehicles.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FForgeVehiclesWaterCraftModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
