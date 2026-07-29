// Copyright Impact-Forge. Vehicle damage model implementation.

#include "Data/ForgeVehicleDamageModel.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FForgeDamageModuleDef* UForgeVehicleDamageModel::FindModule(const FName& ModuleId) const
{
	return Modules.FindByPredicate([&ModuleId](const FForgeDamageModuleDef& Module)
	{
		return Module.ModuleId == ModuleId;
	});
}

#if WITH_EDITOR
EDataValidationResult UForgeVehicleDamageModel::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TSet<FName> Seen;
	for (const FForgeDamageModuleDef& Module : Modules)
	{
		if (Module.ModuleId.IsNone())
		{
			Context.AddError(NSLOCTEXT("ForgeArmor", "ModuleIdMissing", "Damage module has no ModuleId."));
			Result = EDataValidationResult::Invalid;
		}
		else if (Seen.Contains(Module.ModuleId))
		{
			Context.AddError(FText::Format(NSLOCTEXT("ForgeArmor", "ModuleIdDuplicate", "Duplicate damage module id {0}."), FText::FromName(Module.ModuleId)));
			Result = EDataValidationResult::Invalid;
		}
		Seen.Add(Module.ModuleId);

		if (Module.Type == EForgeModuleType::CrewSeat && !Module.CrewRoleTag.IsValid())
		{
			Context.AddError(FText::Format(NSLOCTEXT("ForgeArmor", "CrewRoleMissing", "Crew seat module {0} has no crew role tag."), FText::FromName(Module.ModuleId)));
			Result = EDataValidationResult::Invalid;
		}
		if (Module.Type == EForgeModuleType::Wheel && Module.WheelIndex < 0)
		{
			Context.AddError(FText::Format(NSLOCTEXT("ForgeArmor", "WheelIndexMissing", "Wheel module {0} has no wheel index."), FText::FromName(Module.ModuleId)));
			Result = EDataValidationResult::Invalid;
		}
		for (const FName& Dependency : Module.DisablesWith)
		{
			if (!FindModule(Dependency))
			{
				Context.AddError(FText::Format(NSLOCTEXT("ForgeArmor", "ModuleDependencyMissing", "Module {0} depends on missing module {1}."), FText::FromName(Module.ModuleId), FText::FromName(Dependency)));
				Result = EDataValidationResult::Invalid;
			}
		}
	}
	return Result;
}
#endif
