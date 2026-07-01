//Copyright 2024 H.Kallisto

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FForgeVehiclesRotaryWingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
