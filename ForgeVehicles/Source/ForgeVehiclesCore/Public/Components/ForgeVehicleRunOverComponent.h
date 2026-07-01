// Copyright Impact-Forge.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForgeVehicleRunOverComponent.generated.h"

class UPrimitiveComponent;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FForgeVehicleRunOverDelegate, AActor*, RunOverActor, float, ImpactSpeed, const FHitResult&, Hit);

/**
 * Detects when a Forge vehicle runs over a pawn (e.g. a soldier) and applies damage.
 *
 * It listens to hit/overlap events on the vehicle's collision primitive(s); when the other actor is a
 * pawn (and not another vehicle) and the vehicle is travelling faster than MinRunOverSpeed, it applies
 * point damage on the authority and broadcasts OnRunOverActor so gameplay can react (ragdoll, score,
 * blood decals, etc.).
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESCORE_API UForgeVehicleRunOverComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeVehicleRunOverComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	// End UActorComponent interface

	/* Registers an additional collision primitive to watch (beyond the auto-bound root). */
	UFUNCTION(BlueprintCallable, Category = "Forge Vehicle|Run Over")
	void AddCollisionComponent(UPrimitiveComponent* Component);

	/* Broadcast (on all instances) when this vehicle runs over a qualifying actor. */
	UPROPERTY(BlueprintAssignable, Category = "Forge Vehicle|Run Over")
	FForgeVehicleRunOverDelegate OnRunOverActor;

protected:

	UFUNCTION()
	void OnComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/* Shared handling for a candidate run-over contact. */
	void HandlePotentialRunOver(AActor* OtherActor, const FHitResult& Hit);

	/* Whether OtherActor should be treated as a run-over-able target. */
	bool CanRunOver(const AActor* OtherActor) const;

	/* Binds hit/overlap delegates on a primitive and enables hit notifies. */
	void RegisterPrimitive(UPrimitiveComponent* Primitive);

public:

	/* Minimum vehicle speed (cm/s) required to injure a pawn we contact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Vehicle|Run Over", meta = (ClampMin = "0.0"))
	float MinRunOverSpeed = 250.f;

	/* Damage applied at MinRunOverSpeed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Vehicle|Run Over", meta = (ClampMin = "0.0"))
	float BaseRunOverDamage = 40.f;

	/* If true, damage scales linearly with how far the speed exceeds MinRunOverSpeed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Vehicle|Run Over")
	bool bScaleDamageWithSpeed = true;

	/* Speed (cm/s) at which bScaleDamageWithSpeed reaches MaxRunOverDamage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Vehicle|Run Over", meta = (ClampMin = "1.0", EditCondition = "bScaleDamageWithSpeed"))
	float FullDamageSpeed = 1200.f;

	/* Maximum damage applied when scaling with speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Vehicle|Run Over", meta = (ClampMin = "0.0", EditCondition = "bScaleDamageWithSpeed"))
	float MaxRunOverDamage = 150.f;

	/* Damage type to apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Vehicle|Run Over")
	TSubclassOf<UDamageType> RunOverDamageType;

	/* Only actors of this class (or subclasses) can be run over. Defaults to APawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Vehicle|Run Over")
	TSubclassOf<AActor> RunOverActorFilter;

	/* Minimum seconds between injuring the same actor, to avoid multi-hit spam in one pass. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Forge Vehicle|Run Over", meta = (ClampMin = "0.0"))
	float PerActorCooldown = 0.75f;

protected:

	/* Recently run-over actors and the time they were hit, for cooldown gating (not reflected). */
	TMap<TWeakObjectPtr<AActor>, float> RecentlyRunOver;
};
