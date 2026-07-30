// Copyright Impact-Forge. Electronic warfare emitters that deny drone control links.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"

#include "ForgeDroneJammerComponent.generated.h"

class UForgeDroneJammerComponent;

/**
 * Attach to anything that should deny drone control in an area: a vehicle-mounted EW set, a
 * deployable box, a static installation.
 *
 * Jamming is the counter that makes drones a contested capability rather than a free one. An emitter
 * does not destroy drones; it takes the pilot's link away and lets their own failsafe decide what
 * happens next, which is why pushing a jammer forward changes how the other side can fly.
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESDRONES_API UForgeDroneJammerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeDroneJammerComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// End UActorComponent interface

	/* Effective radius, metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Jammer", meta = (ClampMin = "1.0"))
	float RadiusM = 500.f;

	/* Denial strength at the emitter, 0-1. Below 1 the link degrades but is never fully lost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Jammer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Power = 1.f;

	/* How sharply strength falls with distance. 2 is inverse-square-ish; higher is more localised. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Jammer", meta = (ClampMin = "0.1"))
	float FalloffExponent = 2.f;

	/**
	 * Frequency band this emitter denies. Links tagged with a different band ignore it; an empty tag
	 * is broadband and affects everything. Lets a faction field a jammer their own drones can fly
	 * through, or a counter-drone set that only touches commercial bands.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Jammer")
	FGameplayTag BandTag;

	/* Emitters can be powered down (out of fuel, destroyed, deliberately silent to avoid detection). */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Jammer")
	void SetJammerActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Jammer")
	bool IsJammerActive() const { return bActive; }

	/* Denial contributed at a world position, 0-1, for a link on the given band. */
	float ComputeDenialAt(const FVector& WorldPosition, const FGameplayTag& LinkBand) const;

protected:

	UFUNCTION()
	void OnRep_Active();

	/* Replicated so clients can drive emitter FX and warning UI. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Active, Category = "ForgeDrone|Jammer")
	bool bActive = true;
};

/**
 * Registry of active jammers, so a link can evaluate interference without searching the world.
 */
UCLASS()
class FORGEVEHICLESDRONES_API UForgeDroneJammerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	static UForgeDroneJammerSubsystem* Get(const UObject* WorldContextObject);

	void RegisterJammer(UForgeDroneJammerComponent* Jammer);
	void UnregisterJammer(UForgeDroneJammerComponent* Jammer);

	/**
	 * Total denial affecting a link, 0-1.
	 *
	 * Evaluated against whichever end of the link is closer to each emitter: jamming attacks the
	 * weaker end, so parking a jammer next to the enemy's operator is as effective as putting it
	 * over the target.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Jammer")
	float ComputeJamFactor(const FVector& DronePosition, const FVector& OperatorPosition, FGameplayTag LinkBand) const;

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Jammer")
	int32 GetActiveJammerCount() const;

private:

	UPROPERTY()
	TArray<TObjectPtr<UForgeDroneJammerComponent>> Jammers;
};
