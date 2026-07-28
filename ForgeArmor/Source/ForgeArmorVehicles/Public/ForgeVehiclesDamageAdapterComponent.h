// Copyright Impact-Forge. Damage adapter for ForgeVehicles pawns.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ForgeArmorTypes.h"
#include "Interfaces/ForgeVehicleDamageAdapter.h"

#include "ForgeVehiclesDamageAdapterComponent.generated.h"

/**
 * Maps Forge Armor module damage onto a ForgeVehicles pawn:
 *
 *  - engine kill      -> IForgeVehicleMovementInterface::StopEngine
 *  - wheel damage     -> USuspensionComponent stiffness / bZeroForceComponent
 *  - track kill       -> zero-force on that side's suspension components
 *  - turret drive     -> UForgeVehicleTurretMovementComp::RotationRate scaling
 *  - crew resolution  -> AForgeBaseVehicle seat configs (PlayerState -> pawn)
 *  - crew wounds      -> AActor::TakeDamage on the occupant (games with richer
 *                        damage pipelines should subclass and override)
 *
 * The header carries no ForgeVehicles types: when the ForgeVehicles plugin is
 * not present the implementation compiles as an inert stub (WITH_FORGEVEHICLES).
 * All methods are BlueprintNativeEvents, so Blueprint vehicle classes can extend
 * or replace any mapping.
 */
UCLASS(ClassGroup = (ForgeArmor), meta = (BlueprintSpawnableComponent))
class FORGEARMORVEHICLES_API UForgeVehiclesDamageAdapterComponent : public UActorComponent, public IForgeVehicleDamageAdapter
{
	GENERATED_BODY()

public:
	UForgeVehiclesDamageAdapterComponent();

	/** Turret drive base rotation rate captured on BeginPlay (deg/s, yaw/pitch). */
	UPROPERTY(BlueprintReadOnly, Category = "Forge Armor")
	FRotator BaseTurretRotationRate = FRotator::ZeroRotator;

	/** Suspension stiffness captured per wheel on BeginPlay. */
	UPROPERTY(BlueprintReadOnly, Category = "Forge Armor")
	TArray<float> BaseWheelStiffness;

	/** Damage applied to an occupant per joule of wound energy (TakeDamage units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Armor")
	float CrewDamagePerJoule = 0.1f;

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
};
