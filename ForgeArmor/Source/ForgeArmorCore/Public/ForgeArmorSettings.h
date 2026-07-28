// Copyright Impact-Forge. Project settings & dark-launch switches for the armor system.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"

#include "ForgeArmorSettings.generated.h"

class UForgeArmorMaterialSet;

/**
 * Project-wide Forge Armor configuration (Config/DefaultGame.ini).
 * Everything defaults OFF so the system dark-launches: with the master switch
 * disabled, armor zones never tag themselves impenetrable and every projectile
 * behaves exactly as before the plugin was installed.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Forge Armor"))
class FORGEARMORCORE_API UForgeArmorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UForgeArmorSettings();

	static const UForgeArmorSettings* Get();

	/** Master switch. OFF = armor zones inert, no routing, no interior damage. */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor")
	bool bArmorSystemEnabled = false;

	/** Material catalogue used to resolve zone material tags. */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor", meta = (AllowedClasses = "/Script/ForgeArmorCore.ForgeArmorMaterialSet"))
	FSoftObjectPath MaterialSet;

	/** Ammo performance map (ForgeArmorTB asset mapping bullet data assets to ballistic specs). */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor", meta = (AllowedClasses = "/Script/ForgeArmorTB.ForgeAmmoPerformanceMap"))
	FSoftObjectPath AmmoPerformanceMap;

	/** Max plate interactions (penetration continuations + ricochets) per original shot. */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxShotChainDepth = 8;

	// ---- Behind-armor / crew tuning ----------------------------------------

	/** Fragment energy (J) above which a crew member rolls for incapacitation. */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor|Crew", meta = (ClampMin = "0"))
	float CrewIncapacitationEnergyJ = 80.f;

	/** Fragment energy (J) above which a crew member rolls for death. */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor|Crew", meta = (ClampMin = "0"))
	float CrewLethalEnergyJ = 250.f;

	// ---- Fire & cook-off ----------------------------------------------------

	/** Seconds between fire simulation ticks (server). */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor|Fire", meta = (ClampMin = "0.25"))
	float FireTickIntervalSeconds = 1.f;

	/** Damage per fire tick to a burning module (fraction of MaxHP). */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor|Fire", meta = (ClampMin = "0"))
	float FireDamageFractionPerTick = 0.05f;

	/** Crew burn energy (J) delivered per fire tick while their compartment burns. */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor|Fire", meta = (ClampMin = "0"))
	float FireCrewEnergyPerTickJ = 60.f;

	/** Cook-off delay window once an ammo module catches fire (seconds). */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor|Fire", meta = (ClampMin = "0"))
	float CookOffMinSeconds = 5.f;

	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor|Fire", meta = (ClampMin = "0"))
	float CookOffMaxSeconds = 45.f;

	// ---- Debug --------------------------------------------------------------

	/** Draw armor solves, spall cones and module hits (also forge.Armor.Debug cvar). */
	UPROPERTY(EditAnywhere, Config, Category = "Forge Armor|Debug")
	bool bDebugDraw = false;
};
