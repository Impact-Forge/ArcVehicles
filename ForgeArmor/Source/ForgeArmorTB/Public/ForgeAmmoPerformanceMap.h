// Copyright Impact-Forge. Maps Terminal Ballistics ammunition to armor ballistic specs.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ForgeArmorTypes.h"

#include "ForgeAmmoPerformanceMap.generated.h"

class UBulletDataAsset;
struct FTBBulletInfo;
struct FTBBulletPhysicalProperties;

/**
 * Authored armor performance for the project's ammunition natures.
 * Keyed primarily by bullet data asset; a name-keyed fallback covers impacts
 * where only the Terminal Ballistics bullet info survives. Anything unmapped
 * gets a conservative DeMarre estimate from the bullet's physical properties
 * so every projectile interacts with armor sensibly from day one.
 */
UCLASS(BlueprintType)
class FORGEARMORTB_API UForgeAmmoPerformanceMap : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Authored specs keyed by bullet data asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	TMap<TSoftObjectPtr<UBulletDataAsset>, FForgeAmmoBallisticSpec> ByBulletAsset;

	/** Fallback keyed by TB bullet name (for impacts without a data asset payload). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	TMap<FName, FForgeAmmoBallisticSpec> ByBulletName;

	/**
	 * Resolve the ballistic spec for an impacting bullet.
	 * @param BulletAsset   the data asset carried in the launch payload (may be null)
	 * @param BulletInfo    TB bullet info from the impact
	 * @param Properties    TB physical properties from the impact (estimation fallback)
	 */
	FForgeAmmoBallisticSpec Resolve(const UBulletDataAsset* BulletAsset, const FTBBulletInfo& BulletInfo, const FTBBulletPhysicalProperties& Properties) const;

	/** Estimate a spec purely from TB physical properties (no authored data). */
	static FForgeAmmoBallisticSpec EstimateFromProperties(const FTBBulletPhysicalProperties& Properties);
};
