// Copyright Impact-Forge. ForgeArmorBSP module interface.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FForgeArmorBSPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
