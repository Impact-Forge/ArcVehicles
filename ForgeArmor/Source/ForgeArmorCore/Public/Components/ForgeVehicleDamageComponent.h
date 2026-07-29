// Copyright Impact-Forge. Server-authoritative per-module vehicle damage controller.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "ForgeArmorTypes.h"

#include "ForgeVehicleDamageComponent.generated.h"

class UForgeVehicleDamageModel;

/** An energy release inside the hull (spall cone, residual jet, scab). */
USTRUCT(BlueprintType)
struct FORGEARMORCORE_API FForgeInteriorEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Interior")
	FVector OriginWS = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Interior")
	FVector DirectionWS = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite, Category = "Interior")
	double ConeHalfAngleDeg = 20.0;

	UPROPERTY(BlueprintReadWrite, Category = "Interior")
	int32 FragmentCount = 8;

	UPROPERTY(BlueprintReadWrite, Category = "Interior")
	double TotalEnergyJ = 0.0;

	UPROPERTY(BlueprintReadWrite, Category = "Interior")
	bool bIncendiary = false;

	UPROPERTY(BlueprintReadWrite, Category = "Interior")
	TObjectPtr<AController> InstigatorController = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Interior")
	TObjectPtr<AActor> DamageCauser = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Interior")
	FName SourceZoneId;
};

USTRUCT()
struct FORGEARMORCORE_API FForgeModuleStateArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FForgeModuleRuntimeState> Items;

	TWeakObjectPtr<class UForgeVehicleDamageComponent> Owner;

	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FForgeModuleRuntimeState, FForgeModuleStateArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FForgeModuleStateArray> : public TStructOpsTypeTraitsBase2<FForgeModuleStateArray>
{
	enum { WithNetDeltaSerializer = true };
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeModuleStateChanged, const FForgeModuleRuntimeState&, ModuleState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeKillStateChanged, EForgeVehicleKillState, KillState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeArmorImpactFX, const FForgeArmorImpactFXEvent&, FXEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnForgeModuleFireChanged, FName, ModuleId, bool, bOnFire);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeAmmoDetonation, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeCrewWound, const FForgeCrewWound&, Wound);

/**
 * Owns the runtime damage state of one vehicle: per-module HP/states (replicated
 * fast array), the vehicle kill-state machine, fire & ammo cook-off simulation
 * and crew casualty resolution. Interior damage arrives from the armor component
 * (spall/APHE) and from the registry's warhead evaluators (HEAT/HESH/HE).
 *
 * Functional consequences are pushed through IForgeVehicleDamageAdapter, which
 * concrete vehicle implementations (ForgeVehicles, BattleSpacePlatforms, ...)
 * provide. FX are surfaced as BlueprintAssignable delegates plus an unreliable
 * multicast so clients can play effects without owning any damage logic.
 */
UCLASS(ClassGroup = (ForgeArmor), meta = (BlueprintSpawnableComponent))
class FORGEARMORCORE_API UForgeVehicleDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForgeVehicleDamageComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Armor")
	TObjectPtr<UForgeVehicleDamageModel> DamageModel;

	// ---- FX / gameplay surface ---------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Forge Armor")
	FOnForgeModuleStateChanged OnModuleStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Forge Armor")
	FOnForgeKillStateChanged OnKillStateChanged;

	/** Fired on server AND all clients (via multicast) for every armor interaction. */
	UPROPERTY(BlueprintAssignable, Category = "Forge Armor")
	FOnForgeArmorImpactFX OnImpactFX;

	UPROPERTY(BlueprintAssignable, Category = "Forge Armor")
	FOnForgeModuleFireChanged OnModuleFireChanged;

	UPROPERTY(BlueprintAssignable, Category = "Forge Armor")
	FOnForgeAmmoDetonation OnAmmoDetonation;

	/** Server only: a crew member took a wound (adapter has already been notified). */
	UPROPERTY(BlueprintAssignable, Category = "Forge Armor")
	FOnForgeCrewWound OnCrewWound;

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Interior damage entry points (server) ------------------------------

	/** Spall cone / residual jet / HESH scab into the hull. */
	void ProcessBehindArmor(const FForgeInteriorEvent& Event);

	/** APHE bursting charge detonating inside the hull. */
	void ProcessAPHEBurst(const FVector& OriginWS, float ChargeGrams, AController* InstigatorController, AActor* DamageCauser);

	/** Non-penetrating impact rattling externally mounted modules near the point. */
	void ProcessExternalHit(const FVector& OriginWS, double EnergyJ, double RadiusCm, AController* InstigatorController, AActor* DamageCauser);

	/** External blast (HE shell / mine). Reaches internals only on open-topped vehicles. */
	void ApplyBlast(const FVector& OriginWS, float TNTEquivalentKG, float RadiusCm, AController* InstigatorController, AActor* DamageCauser);

	/** Direct scripted damage to one module (HP units). */
	UFUNCTION(BlueprintCallable, Category = "Forge Armor", meta = (DisplayName = "Apply Module Damage"))
	void ApplyModuleDamage(FName ModuleId, float DamageHP, AController* InstigatorController);

	/** Put out all fires (crew action / extinguisher system). */
	UFUNCTION(BlueprintCallable, Category = "Forge Armor")
	void ExtinguishFires();

	/** Send the FX event to every client (server). Also fires OnImpactFX locally. */
	void BroadcastImpactFX(const FForgeArmorImpactFXEvent& FXEvent);

	// ---- Queries ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Forge Armor")
	EForgeVehicleKillState GetKillState() const { return KillState; }

	UFUNCTION(BlueprintCallable, Category = "Forge Armor")
	bool GetModuleState(FName ModuleId, FForgeModuleRuntimeState& OutState) const;

	UFUNCTION(BlueprintCallable, Category = "Forge Armor")
	float GetStructuralHPFraction() const;

	void OnModuleStateReplicated(const FForgeModuleRuntimeState& ModuleState);

protected:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpactFX(FForgeArmorImpactFXEvent FXEvent);

	UFUNCTION()
	void OnRep_KillState();

	/** World transform of a module volume (socket + local offset). */
	FTransform ResolveModuleTransform(const FForgeDamageModuleDef& Def) const;

	FForgeModuleRuntimeState* FindModuleState(const FName& ModuleId);
	const FForgeModuleRuntimeState* FindModuleState(const FName& ModuleId) const;

	/** Apply energy (J) to a module: HP loss, state transitions, fire rolls. */
	void ApplyModuleEnergy(const FForgeDamageModuleDef& Def, FForgeModuleRuntimeState& State, double EnergyJ, double PerFragmentEnergyJ, bool bIncendiary, AController* InstigatorController);

	/** Crew seat hit: build the wound, roll casualties, notify the adapter. */
	void ApplyCrewHit(const FForgeDamageModuleDef& Def, FForgeModuleRuntimeState& State, double EnergyJ, AController* InstigatorController);

	/** Push module functional consequences through the vehicle adapter. */
	void ApplyAdapterEffects(const FForgeDamageModuleDef& Def, const FForgeModuleRuntimeState& State);

	/** Recompute the vehicle kill state from module states + structural HP. */
	void UpdateKillState();

	void SetModuleOnFire(const FForgeDamageModuleDef& Def, FForgeModuleRuntimeState& State);
	void FireTick();
	void DetonateAmmoModule(FName ModuleId);

	/** Resolve the damage adapter on the owner (actor first, then components). */
	UObject* ResolveAdapter() const;

	/** State -> functional output scale (engine power, traverse speed...). */
	static float StateToEffectScale(EForgeModuleState State);

	UPROPERTY(Replicated)
	FForgeModuleStateArray ModuleStates;

	UPROPERTY(ReplicatedUsing = OnRep_KillState)
	EForgeVehicleKillState KillState = EForgeVehicleKillState::Operational;

	UPROPERTY(Replicated)
	float StructuralHP = 1000.f;

	FTimerHandle FireTimerHandle;
	TMap<FName, FTimerHandle> CookOffTimers;

	/** Cached adapter object implementing IForgeVehicleDamageAdapter. */
	UPROPERTY()
	TObjectPtr<UObject> CachedAdapter;
};
