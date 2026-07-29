// Copyright Impact-Forge. Armor material catalogue: RHA-equivalence values per PhysMat tag.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ForgeArmorTypes.h"

#include "ForgeArmorMaterialSet.generated.h"

/**
 * Catalogue of armor materials referenced by armor profiles.
 * Ships with real-world-anchored defaults (RHA = 1.0 baseline); designers can
 * retune or extend per project.
 */
UCLASS(BlueprintType)
class FORGEARMORCORE_API UForgeArmorMaterialSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UForgeArmorMaterialSet();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials", meta = (TitleProperty = "MaterialTag"))
	TArray<FForgeArmorMaterialSpec> Materials;

	/** Find a material by its PhysMat.* tag. Returns RHA defaults when missing. */
	UFUNCTION(BlueprintCallable, Category = "Forge Armor")
	FForgeArmorMaterialSpec FindMaterial(FGameplayTag MaterialTag, bool& bFound) const;

	const FForgeArmorMaterialSpec* FindMaterialPtr(const FGameplayTag& MaterialTag) const;

	/** Fill Materials with the built-in real-world-anchored catalogue (replaces content). */
	UFUNCTION(CallInEditor, Category = "Materials")
	void PopulateDefaults();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
