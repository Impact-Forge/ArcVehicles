//Copyright 2025 P.Kallisto 


#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FForgeVehiclesGroundModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};
