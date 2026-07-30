// Copyright Impact-Forge. Warhead arming and fuzing for strike drones.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/HitResult.h"

#include "ForgeDroneWarheadComponent.generated.h"

class UDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnForgeWarheadDetonated, const FHitResult&, Impact, UForgeDroneWarheadComponent*, Warhead);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeWarheadArmedChanged, bool, bArmed);

/**
 * The warhead on a kamikaze quad or loitering munition: arming rules and a fuze.
 *
 * This component owns *when* a warhead goes off, not what that does. Arming needs a delay and a
 * minimum distance from launch so a drone cannot detonate in its operator's hands, and cannot be used
 * as a point-blank grenade the instant it leaves them. When the fuze trips, OnDetonated fires and the
 * game decides the effect - which keeps damage modelling (blast, shaped charge, armour penetration)
 * out of the vehicle plugin entirely and lets a project bind its own damage system.
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESDRONES_API UForgeDroneWarheadComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeDroneWarheadComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// End UActorComponent interface

	/* Seconds after arming is requested before the fuze becomes live. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Warhead", meta = (ClampMin = "0.0"))
	float ArmDelaySeconds = 2.f;

	/* Distance from the launch point required before the fuze becomes live, metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Warhead", meta = (ClampMin = "0.0"))
	float MinArmDistanceM = 30.f;

	/* Arm as soon as the drone is deployed rather than waiting for a pilot command. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Warhead")
	bool bAutoArmOnDeploy = false;

	/* Impact speed below which the fuze will not trip, m/s. Stops a gentle bump detonating it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Warhead", meta = (ClampMin = "0.0"))
	float MinImpactSpeedMS = 4.f;

	/* Optional proximity fuze radius, metres. Zero means contact only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Warhead", meta = (ClampMin = "0.0"))
	float ProximityFuzeRadiusM = 0.f;

	/* Destroy the drone when the warhead goes off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Warhead")
	bool bDestroyOwnerOnDetonate = true;

	/**
	 * Opaque payload description handed to whatever binds OnDetonated - a blast profile, a shaped
	 * charge spec, whatever the project's damage system uses. Untyped on purpose: the vehicle plugin
	 * must not depend on a damage plugin to describe a warhead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Warhead")
	TObjectPtr<UDataAsset> WarheadPayloadData;

	/* Fired on the server when the fuze trips. Bind this to apply the actual effect. */
	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Warhead")
	FOnForgeWarheadDetonated OnDetonated;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Warhead")
	FOnForgeWarheadArmedChanged OnArmedChanged;

	/* Request arming. The fuze goes live once the delay and distance conditions are both met. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Warhead")
	void RequestArm();

	/* Safe the warhead again. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Warhead")
	void Safe();

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Warhead")
	bool IsArmed() const { return bArmed; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Warhead")
	bool IsArmingRequested() const { return bArmingRequested; }

	/* Detonate now, regardless of the fuze (command destruct, cook-off). */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Warhead")
	void Detonate(const FHitResult& Impact);

	/* Where the drone was when arming was requested; the minimum distance is measured from here. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Warhead")
	void SetLaunchLocation(const FVector& InLaunchLocation) { LaunchLocation = InLaunchLocation; }

protected:

	UFUNCTION()
	void OnRep_Armed();

	UFUNCTION()
	void HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	/* True once both the delay and the distance conditions are satisfied. */
	bool AreArmingConditionsMet() const;

	void SetArmed(bool bNewArmed);

	UPROPERTY(ReplicatedUsing = OnRep_Armed)
	bool bArmed = false;

	bool bArmingRequested = false;
	bool bDetonated = false;

	float TimeSinceArmRequest = 0.f;
	FVector LaunchLocation = FVector::ZeroVector;
};
