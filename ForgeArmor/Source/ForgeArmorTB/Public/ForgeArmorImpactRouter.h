// Copyright Impact-Forge. Routes Terminal Ballistics impact events into the armor system.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/TBImpactParams.h"
#include "Types/TBProjectileId.h"
#include "Types/TBProjectileInjury.h"

#include "ForgeArmorImpactRouter.generated.h"

/**
 * Bookkeeping for projectile continuation chains: when armor re-fires a bullet
 * (perforation residual or ricochet) the new projectile id is recorded with its
 * chain depth so a single shot can never bounce or penetrate endlessly.
 */
UCLASS()
class FORGEARMORTB_API UForgeArmorChainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	int32 GetChainDepth(const FTBProjectileId& Id) const;
	void RecordChild(const FTBProjectileId& Child, int32 Depth);

private:
	/** Keyed by the id's underlying guid (FTBProjectileId's hash isn't exported by TB). */
	TMap<FGuid, int32> ChainDepths;
};

/**
 * Static entry points the game mode calls from its Terminal Ballistics events.
 * Wire-up (see the integration guide):
 *
 *   BulletImpactEvent  -> RouteBulletImpact(...)  - armor plate hits (stopped at the plate)
 *   BulletInjureEvent  -> RouteBulletInjure(...)  - pass-through hits on external parts (tires, optics)
 *
 * Both return true when the impact belonged to an armored vehicle and was fully
 * handled; the game mode should then skip its own handling for that event.
 * With the armor system disabled they return false immediately.
 */
UCLASS()
class FORGEARMORTB_API UForgeArmorImpactRouter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Forge Armor", meta = (WorldContext = "WorldContextObject"))
	static bool RouteBulletImpact(const UObject* WorldContextObject, const FTBImpactParams& ImpactParams);

	UFUNCTION(BlueprintCallable, Category = "Forge Armor", meta = (WorldContext = "WorldContextObject"))
	static bool RouteBulletInjure(const UObject* WorldContextObject, const FTBImpactParams& ImpactParams, const FTBProjectileInjuryParams& InjuryParams, bool bExited, const FHitResult& ExitHit);
};
