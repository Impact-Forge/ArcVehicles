// Copyright Impact-Forge. Damage adapter for BattleSpacePlatforms (BSP) vehicles.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ForgeArmorTypes.h"
#include "GameplayTagContainer.h"
#include "Interfaces/ForgeVehicleDamageAdapter.h"
#include "Templates/SubclassOf.h"

#include "ForgeArmorBSPAdapterComponent.generated.h"

class UGameplayEffect;

/**
 * Maps Forge Armor module damage onto a BattleSpacePlatforms vehicle
 * (ABSP_VehicleBase and friends):
 *
 *  - engine kill      -> UBSP_EngineComponent ignition stop
 *  - wheel damage     -> BSP wheel components (suspension zero-force / stiffness)
 *  - turret drive     -> attached ABSP_Turret turn-rate scaling
 *  - crew resolution  -> UBSP_Seat occupants (component order = seat index)
 *  - crew wounds      -> occupant's ability system via the crew wound effect
 *  - hull mirror      -> aggregate structural damage pushed to the vehicle's own
 *                        ability system (UPlatformAttributeSet.Health) as a
 *                        set-by-caller gameplay effect
 *
 * The header carries no BattleSpacePlatforms types: outside the BattleSpace
 * project the implementation compiles as an inert stub (WITH_BSPPLATFORMS).
 * All methods are BlueprintNativeEvents - vehicle Blueprints can extend them.
 */
UCLASS(ClassGroup = (ForgeArmor), meta = (BlueprintSpawnableComponent))
class FORGEARMORBSP_API UForgeArmorBSPAdapterComponent : public UActorComponent, public IForgeVehicleDamageAdapter
{
	GENERATED_BODY()

public:
	UForgeArmorBSPAdapterComponent();

	/** Effect that reduces the vehicle's UPlatformAttributeSet.Health. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Armor|GAS")
	TSubclassOf<UGameplayEffect> HullDamageEffect;

	/** Set-by-caller tag carrying the hull damage magnitude. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Armor|GAS")
	FGameplayTag HullDamageSetByCallerTag;

	/** Hull health lost when the vehicle goes from pristine to structurally destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Armor|GAS")
	float HullHealthPerStructuralLoss = 1000.f;

	/** Effect applied to a wounded crew member's ability system (spall, fire, jets). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Armor|GAS")
	TSubclassOf<UGameplayEffect> CrewWoundEffect;

	/** Set-by-caller tag carrying the crew wound magnitude (wound energy in J). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Armor|GAS")
	FGameplayTag CrewWoundSetByCallerTag;

	//~ UActorComponent
	virtual void BeginPlay() override;

	//~ IForgeVehicleDamageAdapter
	virtual void ApplyEngineState_Implementation(float PowerScale, bool bKilled) override;
	virtual void ApplyWheelState_Implementation(int32 WheelIndex, float StiffnessScale, bool bDisabled) override;
	virtual void ApplyTrackState_Implementation(bool bLeftTrack, bool bDisabled) override;
	virtual void ApplyTurretDriveState_Implementation(float TraverseScale, float ElevationScale) override;
	virtual void ApplyGunState_Implementation(bool bCanFire) override;
	virtual APawn* GetCrewOccupant_Implementation(int32 SeatIndex) const override;
	virtual void ApplyCrewWound_Implementation(const FForgeCrewWound& Wound) override;
	virtual void NotifyKillStateChanged_Implementation(EForgeVehicleKillState NewState) override;

protected:
	/** Mirror aggregate structural damage into the vehicle's Health attribute. */
	UFUNCTION()
	void HandleModuleStateChanged(const FForgeModuleRuntimeState& ModuleState);

	void PushHullDamage(float Magnitude);

	/** Base turret turn rates captured on BeginPlay (reflection; BSP keeps them protected). */
	float BaseTurretTurnRate = 0.f;
	float BaseMantletTurnRate = 0.f;

	float LastStructuralFraction = 1.f;
};
