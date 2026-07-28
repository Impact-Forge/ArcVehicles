// Copyright Impact-Forge. Armor material catalogue implementation.

#include "Data/ForgeArmorMaterialSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	FForgeArmorMaterialSpec MakeMaterial(const TCHAR* TagName, EPhysicalSurface SurfaceType, float KE, float CE, float Spall, float HESH, float Density)
	{
		FForgeArmorMaterialSpec Spec;
		Spec.MaterialTag = FGameplayTag::RequestGameplayTag(FName(TagName), /*ErrorIfNotFound*/ false);
		Spec.SurfaceType = SurfaceType;
		Spec.RHAeVsKE = KE;
		Spec.RHAeVsCE = CE;
		Spec.SpallSuppression = Spall;
		Spec.HESHFactor = HESH;
		Spec.DensityGCm3 = Density;
		return Spec;
	}
}

UForgeArmorMaterialSet::UForgeArmorMaterialSet()
{
	// Defaults are seeded via the PopulateDefaults editor button rather than in
	// the constructor: gameplay tags are not registered yet at CDO construction
	// time, so tag requests here would silently bake empty tags into the asset.
}

void UForgeArmorMaterialSet::PopulateDefaults()
{
	Materials.Reset();

	// Baseline steels. Surface types follow the BattleSpace project surface table
	// (31 ArmoredSteel, 42 VehicleArmor, 15 HardenedSteel, ...); harmless defaults
	// elsewhere - the surface type only feeds impact FX.
	Materials.Add(MakeMaterial(TEXT("PhysMat.ArmoredSteel"),     SurfaceType31, 1.00f, 1.00f, 0.00f, 1.00f, 7.85f)); // RHA baseline
	Materials.Add(MakeMaterial(TEXT("PhysMat.VehicleArmor"),     SurfaceType42, 1.05f, 1.05f, 0.00f, 1.00f, 7.85f)); // generic modern welded
	Materials.Add(MakeMaterial(TEXT("PhysMat.HardenedSteel"),    SurfaceType15, 1.15f, 1.00f, 0.00f, 1.00f, 7.85f)); // HHS (thin plates)
	Materials.Add(MakeMaterial(TEXT("PhysMat.CastIron"),         SurfaceType8,  0.94f, 0.94f, 0.00f, 1.00f, 7.20f)); // cast armor
	Materials.Add(MakeMaterial(TEXT("PhysMat.Aluminum"),         SurfaceType2,  0.30f, 0.25f, 0.00f, 0.60f, 2.70f)); // 5083-class hulls
	Materials.Add(MakeMaterial(TEXT("PhysMat.Titanium"),         SurfaceType_Default, 0.85f, 0.80f, 0.00f, 0.80f, 4.50f));

	// Composites and ceramics: strong vs CE, moderate vs KE, decouple HESH.
	Materials.Add(MakeMaterial(TEXT("PhysMat.CompositeArmor"),   SurfaceType50, 1.25f, 2.00f, 0.20f, 0.15f, 4.00f));
	Materials.Add(MakeMaterial(TEXT("PhysMat.CeramicComposite"), SurfaceType32, 1.40f, 1.90f, 0.10f, 0.15f, 3.60f));
	Materials.Add(MakeMaterial(TEXT("PhysMat.BoronCarbide"),     SurfaceType59, 1.60f, 2.10f, 0.10f, 0.15f, 2.52f));
	Materials.Add(MakeMaterial(TEXT("PhysMat.SiliconCarbide"),   SurfaceType60, 1.50f, 2.00f, 0.10f, 0.15f, 3.21f));
	Materials.Add(MakeMaterial(TEXT("PhysMat.DepletedUranium"),  SurfaceType47, 1.30f, 1.50f, 0.00f, 0.50f, 19.05f));

	// Fabric liners: near-zero plate value, high spall suppression.
	Materials.Add(MakeMaterial(TEXT("PhysMat.Aramid"),           SurfaceType58, 0.25f, 0.45f, 0.50f, 0.15f, 1.44f));
	{
		FForgeArmorMaterialSpec SpallLiner = MakeMaterial(TEXT("PhysMat.SpallLiner"), SurfaceType61, 0.10f, 1.00f, 0.70f, 0.15f, 1.20f);
		Materials.Add(SpallLiner);
	}

	// Explosive reactive armor: consumable tiles.
	{
		FForgeArmorMaterialSpec ERA = MakeMaterial(TEXT("PhysMat.ReactiveArmor"), SurfaceType48, 0.15f, 0.15f, 0.00f, 0.15f, 2.50f);
		ERA.bIsERA = true;
		ERA.ERACutKEMM = 150.f;   // heavy ERA vs full-bore KE
		ERA.ERACutCEMM = 300.f;   // vs shaped charge jets
		ERA.ERALongRodFactor = 0.7f;
		Materials.Add(ERA);
	}
}

FForgeArmorMaterialSpec UForgeArmorMaterialSet::FindMaterial(FGameplayTag MaterialTag, bool& bFound) const
{
	if (const FForgeArmorMaterialSpec* Spec = FindMaterialPtr(MaterialTag))
	{
		bFound = true;
		return *Spec;
	}
	bFound = false;
	return FForgeArmorMaterialSpec(); // RHA defaults
}

const FForgeArmorMaterialSpec* UForgeArmorMaterialSet::FindMaterialPtr(const FGameplayTag& MaterialTag) const
{
	return Materials.FindByPredicate([&MaterialTag](const FForgeArmorMaterialSpec& Spec)
	{
		return Spec.MaterialTag == MaterialTag;
	});
}

#if WITH_EDITOR
EDataValidationResult UForgeArmorMaterialSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TSet<FGameplayTag> Seen;
	for (const FForgeArmorMaterialSpec& Spec : Materials)
	{
		if (!Spec.MaterialTag.IsValid())
		{
			Context.AddError(NSLOCTEXT("ForgeArmor", "MaterialTagMissing", "Material entry has no PhysMat tag."));
			Result = EDataValidationResult::Invalid;
		}
		else if (Seen.Contains(Spec.MaterialTag))
		{
			Context.AddError(FText::Format(NSLOCTEXT("ForgeArmor", "MaterialTagDuplicate", "Duplicate material tag {0}."), FText::FromName(Spec.MaterialTag.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
		Seen.Add(Spec.MaterialTag);
	}
	return Result;
}
#endif
