// Copyright Impact-Forge. Vehicle armor orchestrator: zones, plate solves, ERA state.

#pragma once

#include "Ballistics/ForgeArmorBallistics.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "ForgeArmorTypes.h"

#include "ForgeVehicleArmorComponent.generated.h"

class UForgeArmorMaterialSet;
class UForgeArmorProfile;
class UForgeArmorZoneComponent;
class UForgeVehicleDamageComponent;
class UPrimitiveComponent;

/** Replicated runtime state of one armor zone (ERA tiles, plate integrity). */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeArmorZoneState : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	FName ZoneId;

	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	int32 ERATilesRemaining = 0;

	/** Cracked by repeated near-perforations: plate effectiveness reduced. */
	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	bool bCompromised = false;
};

USTRUCT()
struct FORGEARMORCORE_API FForgeArmorZoneStateArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FForgeArmorZoneState> Items;

	/** Owning component, set locally on both sides (not replicated). */
	TWeakObjectPtr<UForgeVehicleArmorComponent> Owner;

	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FForgeArmorZoneState, FForgeArmorZoneStateArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FForgeArmorZoneStateArray> : public TStructOpsTypeTraitsBase2<FForgeArmorZoneStateArray>
{
	enum { WithNetDeltaSerializer = true };
};

/** Everything the ballistics router needs to know about a resolved armor interaction. */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeArmorInteraction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	EForgePenetrationOutcome Outcome = EForgePenetrationOutcome::Stopped;

	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	FName ZoneId;

	/** Penetration available / effective thickness (mm RHA) for feed/debug. */
	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	float PenAvailableMM = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	float EffectiveThicknessMM = 0.f;

	/** True when the round continues (perforation with residual, or ricochet). */
	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	bool bShouldContinue = false;

	/** World location to continue the projectile from. */
	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	FVector ContinuationLocation = FVector::ZeroVector;

	/** Continuation velocity in cm/s (Unreal units, ready for a ballistic re-fire). */
	UPROPERTY(BlueprintReadOnly, Category = "Armor")
	FVector ContinuationVelocityCmS = FVector::ZeroVector;
};

/**
 * Owns the authored armor layout of one vehicle: spawns zone primitives from the
 * armor profile, holds replicated zone state (ERA tiles, plate integrity), and
 * resolves kinetic impacts delivered by the ballistics router into penetrations,
 * ricochets and behind-armor effects (via the damage component).
 *
 * Server authoritative; zone state replicates for client-side visuals.
 */
UCLASS(ClassGroup = (ForgeArmor), meta = (BlueprintSpawnableComponent))
class FORGEARMORCORE_API UForgeVehicleArmorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForgeVehicleArmorComponent();

	/** Armor layout for this vehicle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Armor")
	TObjectPtr<UForgeArmorProfile> ArmorProfile;

	/** Optional material catalogue override (falls back to project settings). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Armor")
	TObjectPtr<UForgeArmorMaterialSet> MaterialSetOverride;

	/** Arm automatically on BeginPlay when the armor system is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Armor")
	bool bAutoArm = true;

	/**
	 * Components carrying this tag keep their projectile collision when the armor
	 * arms (tires, external stores that should stay directly shootable).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Armor")
	FName KeepProjectileCollisionTag = TEXT("ForgeArmor.KeepProjectileCollision");

	/** Fired when a zone's replicated state changes (ERA tile spent, plate cracked). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeZoneStateChanged, const FForgeArmorZoneState&, ZoneState);
	UPROPERTY(BlueprintAssignable, Category = "Forge Armor")
	FOnForgeZoneStateChanged OnZoneStateChanged;

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Arm/disarm all zones (tag them impenetrable + take over projectile collision). */
	UFUNCTION(BlueprintCallable, Category = "Forge Armor")
	void SetArmorArmed(bool bArmed);

	UFUNCTION(BlueprintCallable, Category = "Forge Armor")
	bool IsArmorArmed() const { return bArmorArmed; }

	/**
	 * Resolve a kinetic impact against this vehicle's armor (server only).
	 * Runs the plate stack solve, consumes ERA, applies behind-armor effects
	 * through the damage component and reports how the projectile continues.
	 *
	 * @param HitResult          the impact on one of this vehicle's zones
	 * @param ImpactVelocityCmS  projectile velocity at impact, cm/s
	 * @param AmmoSpec           ballistic spec of the ammunition nature
	 * @param ProjectileMassKG   projectile mass, kg
	 */
	FForgeArmorInteraction HandleKineticImpact(
		const FHitResult& HitResult,
		const FVector& ImpactVelocityCmS,
		const FForgeAmmoBallisticSpec& AmmoSpec,
		double ProjectileMassKG,
		AController* InstigatorController,
		AActor* DamageCauser);

	/** Zone def for a hit component (spawned zones and adopted primitives). Null when unknown. */
	const FForgeArmorZoneDef* ResolveZone(const UPrimitiveComponent* HitComponent) const;

	/** Mutable runtime state of a zone. */
	FForgeArmorZoneState* FindZoneState(const FName& ZoneId);
	const FForgeArmorZoneState* FindZoneState(const FName& ZoneId) const;

	/**
	 * Build the CE path (plates + intact ERA) a shaped-charge jet must defeat when
	 * striking the given component at the given obliquity. Consumes no state.
	 * Returns false when the component is not an armor zone.
	 */
	bool BuildCEPath(const UPrimitiveComponent* HitComponent, double AngleFromNormalDeg, TArray<ForgeArmor::Ballistics::FCEPathElement>& OutPath, double& OutSpallSuppression, FName& OutZoneId) const;

	/** Spend one ERA tile on a zone (server). Returns true if a tile was consumed. */
	bool ConsumeERATile(const FName& ZoneId);

	/** HESH susceptibility factor of the zone's outermost material. */
	double GetHESHFactor(const UPrimitiveComponent* HitComponent) const;

	/** The material catalogue in effect (override or project settings). */
	const UForgeArmorMaterialSet* GetMaterialSet() const;

	UForgeVehicleDamageComponent* GetDamageComponent() const;

	void HandleZoneStateReplicated(const FForgeArmorZoneState& ZoneState);

protected:
	void SpawnZones();
	void DestroyZones();
	void ApplyHullProjectileCollision(bool bIgnoreProjectiles);

	/** Walk the BackingZoneId chain outermost-first (max 4 plates). */
	void GatherZoneStack(const FForgeArmorZoneDef& OuterZone, TArray<const FForgeArmorZoneDef*>& OutStack) const;

	/** Mark a plate cracked after a heavy near-perforation. */
	void CompromiseZone(const FName& ZoneId);

	void DebugDrawInteraction(const FHitResult& HitResult, const FForgeArmorInteraction& Interaction, double AngleFromNormalDeg) const;

	UPROPERTY(Replicated)
	FForgeArmorZoneStateArray ZoneStates;

	UPROPERTY()
	TArray<TObjectPtr<UForgeArmorZoneComponent>> SpawnedZones;

	/** Adopted existing primitives, mapped to their zone ids. */
	TMap<TWeakObjectPtr<const UPrimitiveComponent>, FName> AdoptedZoneComponents;

	/** Hull primitives whose projectile response we suppressed while armed. */
	TArray<TWeakObjectPtr<UPrimitiveComponent>> SuppressedHullComponents;

	UPROPERTY()
	TObjectPtr<UForgeArmorMaterialSet> CachedMaterialSet;

	bool bArmorArmed = false;
};
