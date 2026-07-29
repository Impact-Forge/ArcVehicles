// Copyright Impact-Forge. One armor plate as a collision primitive.

#pragma once

#include "Components/BoxComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"

#include "ForgeArmorZoneComponent.generated.h"

class UForgeVehicleArmorComponent;

namespace ForgeArmor
{
	/**
	 * Component tag that makes the ballistics simulation stop a projectile at this
	 * primitive and report the impact. Defaults to Terminal Ballistics' plain
	 * "IMPENETRABLE" tag; the ForgeArmorTB module re-synchronises it from the TB
	 * API at startup so a TB rename can never silently break the contract.
	 */
	FORGEARMORCORE_API FName GetImpenetrableTagName();
	FORGEARMORCORE_API void SetImpenetrableTagName(const FName& TagName);
}

/**
 * Collision primitive for a single authored armor zone. Spawned (or adopted) by
 * UForgeVehicleArmorComponent. The box's local +Z is the plate normal for
 * authored ChildBox zones; impact obliquity is measured against the hit face
 * normal at runtime.
 *
 * Implements IGameplayTagAssetInterface so the ballistics plugin resolves the
 * zone's PhysMat.* material tag for surface effects.
 */
UCLASS(ClassGroup = (ForgeArmor), meta = (BlueprintSpawnableComponent))
class FORGEARMORCORE_API UForgeArmorZoneComponent : public UBoxComponent, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	UForgeArmorZoneComponent();

	/** Zone id in the owning armor profile. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forge Armor")
	FName ZoneId;

	/** PhysMat.* tag of the zone material (surface resolution + FX). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forge Armor")
	FGameplayTag MaterialPhysMatTag;

	/** Armor component owning this zone. */
	UPROPERTY()
	TWeakObjectPtr<UForgeVehicleArmorComponent> OwningArmor;

	/** Add/remove the impenetrable tag - armed zones stop projectiles for evaluation. */
	void SetArmed(bool bArmed);
	bool IsArmed() const;

	/** Configure collision to only block the projectile trace channel. */
	void ConfigureZoneCollision(ECollisionChannel ProjectileChannel);

	//~ IGameplayTagAssetInterface
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
};
