// Copyright Impact-Forge. Shared enums and structs for the Forge Armor damage framework.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/NetSerialization.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "ForgeArmorTypes.generated.h"

class AController;

/**
 * Penetrator classification. Drives the DeMarre exponent, slope-effect table and
 * ricochet behaviour of the kinetic-energy solver, and selects the chemical-energy
 * path for shaped-charge/squash-head/blast warheads.
 */
UENUM(BlueprintType)
enum class EForgePenetratorClass : uint8
{
	/** Ball / full metal jacket small arms. Soft core, poor armor performance. */
	FMJ,
	/** Armor piercing (hardened steel core), incl. AP/APBC small arms and full-bore shot. */
	AP,
	/** Armor piercing incendiary. */
	API,
	/** Capped AP (APC/APCBC). Better slope performance than plain AP. */
	APCBC,
	/** Composite rigid / high velocity AP (APCR/HVAP). Poor slope performance. */
	APCR,
	/** Armor piercing discarding sabot (full-bore subprojectile). */
	APDS,
	/** Long-rod fin stabilised discarding sabot. Nearly slope-insensitive. */
	APFSDS,
	/** Shaped charge jet (chemical energy). Velocity independent. */
	HEAT,
	/** High explosive squash head / plastic. Defeats monolithic plate by scabbing. */
	HESH,
	/** Plain high explosive / blast. */
	HE
};

/** Functional module of a vehicle that can be damaged. */
UENUM(BlueprintType)
enum class EForgeModuleType : uint8
{
	Engine,
	Transmission,
	FuelTank,
	AmmoStowage,
	TurretTraverse,
	GunBreech,
	GunBarrel,
	Optics,
	Radio,
	TrackLeft,
	TrackRight,
	Wheel,
	Radiator,
	CrewSeat,
	/** Generic structural hitpoints (catch-all interior). */
	Structure
};

/** Health state of a single damage module. */
UENUM(BlueprintType)
enum class EForgeModuleState : uint8
{
	Nominal,
	Damaged,
	Critical,
	Destroyed
};

/** Aggregate kill state of the vehicle. */
UENUM(BlueprintType)
enum class EForgeVehicleKillState : uint8
{
	Operational,
	MobilityKill,
	FirepowerKill,
	MobilityAndFirepowerKill,
	Destroyed,
	/** Ammo detonation / catastrophic loss (turret toss class of outcome). */
	DestroyedCatastrophic
};

/** How an armor zone acquires its collision primitive. */
UENUM(BlueprintType)
enum class EForgeArmorZoneBinding : uint8
{
	/** Spawn a UForgeArmorZoneComponent box attached to a socket/bone. */
	ChildBox,
	/** Adopt an existing primitive component found by component tag. */
	ExistingComponent
};

/** Outcome of a single armor interaction. */
UENUM(BlueprintType)
enum class EForgePenetrationOutcome : uint8
{
	/** Round defeated, no interior effect (may still have partial back-face spall). */
	Stopped,
	/** Round perforated the plate with residual velocity. */
	Penetrated,
	/** Round deflected. */
	Ricochet
};

/**
 * Ballistic performance of one ammunition nature against armor.
 * Authored against real-world penetration data: PenetrationRefMM is the flat
 * (0 deg obliquity) RHA penetration at ReferenceSpeedMS.
 */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeAmmoBallisticSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	EForgePenetratorClass PenetratorClass = EForgePenetratorClass::FMJ;

	/** Flat RHA penetration (mm) at ReferenceSpeedMS, 0 deg obliquity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0"))
	float PenetrationRefMM = 5.f;

	/** Speed (m/s) the reference penetration is quoted at (usually muzzle velocity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "1"))
	float ReferenceSpeedMS = 800.f;

	/** Projectile caliber in mm (for overmatch and HESH scab rules). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "1"))
	float CaliberMM = 7.62f;

	/** Optional DeMarre velocity exponent override. <= 0 uses the class default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", AdvancedDisplay)
	float DeMarreExponentOverride = 0.f;

	/** True for APHE-style rounds with a bursting charge behind the penetrator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo|APHE")
	bool bHasBurstingCharge = false;

	/** Minimum plate (mm RHA) that arms the fuze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo|APHE", meta = (EditCondition = "bHasBurstingCharge"))
	float FuzeArmThresholdMM = 12.f;

	/** Distance (m) travelled past the armed plate before the burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo|APHE", meta = (EditCondition = "bHasBurstingCharge"))
	float FuzeTravelM = 1.3f;

	/** Bursting charge, grams of TNT equivalent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo|APHE", meta = (EditCondition = "bHasBurstingCharge"))
	float BurstChargeGrams = 20.f;

	/** Incendiary content: raises fire ignition probability on fuel/engine hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	bool bIncendiary = false;

	bool IsKinetic() const
	{
		return PenetratorClass != EForgePenetratorClass::HEAT
			&& PenetratorClass != EForgePenetratorClass::HESH
			&& PenetratorClass != EForgePenetratorClass::HE;
	}
};

/**
 * One armor material: RHA-equivalence multipliers and behind-armor behaviour.
 * KE and CE effectiveness are separate, mirroring how composite/reactive arrays
 * defeat shaped charges far better than long rods.
 */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeArmorMaterialSpec
{
	GENERATED_BODY()

	/** PhysMat.* gameplay tag identifying this material (also used for TB surface resolution). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (Categories = "PhysMat"))
	FGameplayTag MaterialTag;

	/** Engine surface type reported in impact FX events. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	/** Thickness efficiency vs kinetic penetrators (RHA = 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0"))
	float RHAeVsKE = 1.f;

	/** Thickness efficiency vs shaped-charge jets (RHA = 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0"))
	float RHAeVsCE = 1.f;

	/** 0..1 fraction of spall suppressed behind this material (spall liners ~0.7). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0", ClampMax = "1"))
	float SpallSuppression = 0.f;

	/**
	 * HESH effectiveness against this material. 1 = monolithic steel (full scab),
	 * ~0.15 for spaced/composite/lined arrays that decouple the shock.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0", ClampMax = "1"))
	float HESHFactor = 1.f;

	/** Explosive reactive armor: consumable one-shot tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material|ERA")
	bool bIsERA = false;

	/** mm RHA removed from a kinetic penetrator by an intact tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material|ERA", meta = (EditCondition = "bIsERA"))
	float ERACutKEMM = 0.f;

	/** mm RHA removed from a shaped-charge jet by an intact tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material|ERA", meta = (EditCondition = "bIsERA"))
	float ERACutCEMM = 0.f;

	/** Long rods (APFSDS) are less affected by ERA: scale applied to ERACutKEMM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material|ERA", meta = (EditCondition = "bIsERA", ClampMin = "0", ClampMax = "1"))
	float ERALongRodFactor = 0.7f;

	/** g/cm3, used for mass estimates in FX/debug only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", AdvancedDisplay)
	float DensityGCm3 = 7.85f;
};

/**
 * A single authored armor zone: a plate with a material and a real-world thickness.
 * The zone's collision box orientation defines the plate normal; line-of-sight
 * thickness is computed from the authored mm value and the impact obliquity.
 */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeArmorZoneDef
{
	GENERATED_BODY()

	/** Unique id within the profile (e.g. "Hull.Glacis", "Turret.Front"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	FName ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	EForgeArmorZoneBinding Binding = EForgeArmorZoneBinding::ChildBox;

	/** Socket or bone on the vehicle mesh the zone box attaches to (ChildBox binding). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (EditCondition = "Binding == EForgeArmorZoneBinding::ChildBox"))
	FName AttachSocket = NAME_None;

	/** Component tag identifying an existing primitive to adopt (ExistingComponent binding). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (EditCondition = "Binding == EForgeArmorZoneBinding::ExistingComponent"))
	FName ComponentTag = NAME_None;

	/** Box extent in cm (half sizes), local to the attach point (ChildBox binding). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (EditCondition = "Binding == EForgeArmorZoneBinding::ChildBox"))
	FVector BoxExtentCm = FVector(50.f, 50.f, 2.f);

	/** Local offset/rotation relative to the attach point. Box local +Z is the plate normal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (EditCondition = "Binding == EForgeArmorZoneBinding::ChildBox"))
	FTransform LocalTransform = FTransform::Identity;

	/** Armor material of this plate (must exist in the material set). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (Categories = "PhysMat"))
	FGameplayTag MaterialTag;

	/** Physical plate thickness in real mm (LOS thickness derives from obliquity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (ClampMin = "1"))
	float ThicknessMM = 10.f;

	/** ERA tile count for reactive zones. Each interaction spends one tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (ClampMin = "0"))
	int32 ERATileCount = 0;

	/**
	 * Optional underlying plate behind an applique/ERA zone, referenced by ZoneId.
	 * When a round defeats this zone it is evaluated against the backing zone next
	 * (spaced-array behaviour without geometric stacking).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	FName BackingZoneId = NAME_None;

	/** Human readable name for kill feed / debug. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	FText DisplayName;
};

/** One functional damage module of the vehicle interior. */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeDamageModuleDef
{
	GENERATED_BODY()

	/** Unique id within the damage model (e.g. "Engine", "AmmoRack.Hull", "Crew.Driver"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	FName ModuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	EForgeModuleType Type = EForgeModuleType::Structure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (ClampMin = "1"))
	float MaxHP = 100.f;

	/** Socket/bone on the vehicle mesh placing the module volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	FName AttachSocket = NAME_None;

	/** Local offset from the socket (or actor origin when no socket). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	FTransform LocalTransform = FTransform::Identity;

	/** Module volume half-extents in cm, used by the behind-armor expected-hit model. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	FVector VolumeExtentCm = FVector(30.f, 30.f, 30.f);

	/** Interior energy (J) that takes this module from full HP to zero (spall/jet/blast coupling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (ClampMin = "1"))
	float EnergyCapacityJ = 2000.f;

	/** Damaged / Critical thresholds as HP fractions (state machine cutoffs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (ClampMin = "0", ClampMax = "1"))
	float DamagedThreshold = 0.66f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (ClampMin = "0", ClampMax = "1"))
	float CriticalThreshold = 0.33f;

	/** Modules that stop functioning when this one is destroyed (by ModuleId). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	TArray<FName> DisablesWith;

	/** 0..1 chance a damaging hit starts a fire at this module (scaled by ammo flags). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (ClampMin = "0", ClampMax = "1"))
	float Flammability = 0.f;

	/** Wet stowage / blast-protected ammo halves cook-off probability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (EditCondition = "Type == EForgeModuleType::AmmoStowage"))
	bool bWetStowage = false;

	/** Index into the vehicle's wheel/suspension array (Wheel modules). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (EditCondition = "Type == EForgeModuleType::Wheel"))
	int32 WheelIndex = INDEX_NONE;

	/** Crew role occupying this station (CrewSeat modules), e.g. Vehicle.Crew.Driver. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (EditCondition = "Type == EForgeModuleType::CrewSeat"))
	FGameplayTag CrewRoleTag;

	/** Seat identifier used by the seat adapter to resolve the occupant (CrewSeat modules). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module", meta = (EditCondition = "Type == EForgeModuleType::CrewSeat"))
	int32 SeatIndex = INDEX_NONE;

	/** True for modules mounted outside the armor (optics, tracks, external tanks): damaged by non-penetrating hits nearby. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	bool bExternal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	FText DisplayName;
};

/** Replicated runtime state of one damage module. */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeModuleRuntimeState : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Module")
	FName ModuleId;

	UPROPERTY(BlueprintReadOnly, Category = "Module")
	EForgeModuleState State = EForgeModuleState::Nominal;

	/** Current HP as a fraction of MaxHP. */
	UPROPERTY(BlueprintReadOnly, Category = "Module")
	float HPFraction = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Module")
	bool bOnFire = false;
};

/** A wound to be applied to a crew member through the seat adapter. */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeCrewWound
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Crew")
	FName SeatModuleId;

	UPROPERTY(BlueprintReadWrite, Category = "Crew")
	int32 SeatIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Crew")
	FGameplayTag CrewRoleTag;

	/** Fragment/jet energy delivered to the crew member (J). */
	UPROPERTY(BlueprintReadWrite, Category = "Crew")
	float EnergyJ = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Crew")
	bool bLethalRoll = false;

	UPROPERTY(BlueprintReadWrite, Category = "Crew")
	TObjectPtr<AController> InstigatorController = nullptr;
};

/** Compact description of an armor interaction for FX + kill feed (multicast to clients). */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeArmorImpactFXEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FX")
	FVector_NetQuantize Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "FX")
	FVector_NetQuantizeNormal Normal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "FX")
	EForgePenetrationOutcome Outcome = EForgePenetrationOutcome::Stopped;

	UPROPERTY(BlueprintReadOnly, Category = "FX")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	/** Caliber in mm, quantized - drives FX scale. */
	UPROPERTY(BlueprintReadOnly, Category = "FX")
	uint8 CaliberClass = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FX")
	FName ZoneId;
};

/** Result of resolving a penetrating hit against the vehicle interior. */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeBehindArmorResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BehindArmor")
	EForgePenetrationOutcome Outcome = EForgePenetrationOutcome::Stopped;

	/** Residual penetrator speed after the plate, m/s (KE) - 0 when stopped. */
	UPROPERTY(BlueprintReadOnly, Category = "BehindArmor")
	float ResidualSpeedMS = 0.f;

	/** Residual jet penetration after the array, mm RHA (CE). */
	UPROPERTY(BlueprintReadOnly, Category = "BehindArmor")
	float ResidualPenetrationMM = 0.f;

	/** Energy released into the interior (J). */
	UPROPERTY(BlueprintReadOnly, Category = "BehindArmor")
	float InteriorEnergyJ = 0.f;

	/** Modules damaged, by id. */
	UPROPERTY(BlueprintReadOnly, Category = "BehindArmor")
	TArray<FName> DamagedModules;
};
