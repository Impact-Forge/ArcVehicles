// Copyright Impact-Forge. Loitering munition: launch, loiter, then dive onto a target.

#pragma once

#include "Archetypes/ForgeFixedWingUAV.h"
#include "CoreMinimal.h"

#include "ForgeLoiteringMunition.generated.h"

class UForgeDroneWarheadComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnForgeMunitionCommitted);

/**
 * A loitering munition: a small fixed-wing airframe that waits over an area and then flies itself
 * into a target.
 *
 * Mechanically it is the reconnaissance UAV plus a warhead and one irreversible decision. The loiter
 * phase is ordinary flying; committing to a target is not, because the aircraft is expended either way
 * once it goes. That asymmetry is what makes it a different weapon rather than a UAV that happens to
 * explode, so commitment is an explicit call with its own delegate rather than a flight mode the pilot
 * can idly toggle.
 */
UCLASS()
class FORGEVEHICLESDRONES_API AForgeLoiteringMunition : public AForgeFixedWingUAV
{
	GENERATED_BODY()

public:

	AForgeLoiteringMunition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* The warhead, created with the airframe. */
	UForgeDroneWarheadComponent* GetWarhead() const { return Warhead; }

	/**
	 * Commit to a target and begin the terminal run. Arms the warhead and hands the aircraft to the
	 * autopilot's dive profile. One-way: the aircraft is expended either way once it commits.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeMunition")
	void CommitToTarget(const FVector& TargetLocation);

	/* As above, but tracks a moving target while it lives. */
	UFUNCTION(BlueprintCallable, Category = "ForgeMunition")
	void CommitToTargetActor(AActor* TargetActor);

	/* Break off a run that has not yet armed, and return to loitering. */
	UFUNCTION(BlueprintCallable, Category = "ForgeMunition")
	bool AbortAttack();

	UFUNCTION(BlueprintPure, Category = "ForgeMunition")
	bool HasCommitted() const { return bCommitted; }

	UPROPERTY(BlueprintAssignable, Category = "ForgeMunition")
	FOnForgeMunitionCommitted OnCommitted;

protected:

	/* Dive attitude authority: a diving munition must stay controllable at high speed. */
	virtual void ApplySmallAirframeTuning() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneWarheadComponent> Warhead;

	UPROPERTY(Replicated)
	bool bCommitted = false;
};
