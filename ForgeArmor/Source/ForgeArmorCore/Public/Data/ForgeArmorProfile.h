// Copyright Impact-Forge. Authored armor layout of one vehicle: zones with real mm values.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ForgeArmorTypes.h"

#include "ForgeArmorProfile.generated.h"

/**
 * The armor layout of a vehicle: a set of plates (zones) with materials and
 * real-world thicknesses. Authored from cutaway references the same way War
 * Thunder / GHPC armor viewers present them.
 */
UCLASS(BlueprintType)
class FORGEARMORCORE_API UForgeArmorProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor", meta = (TitleProperty = "ZoneId"))
	TArray<FForgeArmorZoneDef> Zones;

	/** Material used when a hit resolves to no authored zone (catch-all skin). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Fallback", meta = (Categories = "PhysMat"))
	FGameplayTag DefaultMaterialTag;

	/** Thickness (mm) for the catch-all skin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor|Fallback", meta = (ClampMin = "0"))
	float DefaultThicknessMM = 8.f;

	const FForgeArmorZoneDef* FindZone(const FName& ZoneId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
