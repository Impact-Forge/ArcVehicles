// K2 FixedWing Physics

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FForgeVehiclesFixedWingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
