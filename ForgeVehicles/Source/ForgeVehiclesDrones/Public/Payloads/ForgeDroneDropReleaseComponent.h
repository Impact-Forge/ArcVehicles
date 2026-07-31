// Copyright Impact-Forge. Drops munitions or stores from a drone.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "ForgeDroneDropReleaseComponent.generated.h"

class AController;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnForgeDroneStoreReleased, AActor*, ReleasedActor, int32, RemainingStores)
;

/**
 * Releases carried stores - grenades, submunitions, supplies - from a drone in flight.
 *
 * Released actors inherit the drone's velocity, which is what makes accuracy a matter of judging the
 * run-in rather than hovering directly overhead: a store dropped from a moving quad travels with it.
 * The releasing operator is recorded as the instigator so kills credit the pilot, not the airframe.
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESDRONES_API UForgeDroneDropReleaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeDroneDropReleaseComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* What is carried. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Stores")
	TSubclassOf<AActor> StoreClass;

	/* How many are carried. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Stores", meta = (ClampMin = "1"))
	int32 StoreCapacity = 2;

	/* Socket on the drone mesh releases happen from. Falls back to the actor origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Stores")
	FName ReleaseSocket = NAME_None;

	/* Small downward push so a store clears the airframe instead of scraping it, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Stores", meta = (ClampMin = "0.0"))
	float SeparationImpulseCmS = 150.f;

	/* Minimum time between releases, seconds. Stops a whole rack going in one frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Stores", meta = (ClampMin = "0.0"))
	float ReleaseIntervalSeconds = 0.5f;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Stores")
	FOnForgeDroneStoreReleased OnStoreReleased;

	/* Release one store. Server authoritative; returns the spawned actor, or null if it could not. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Stores")
	AActor* ReleaseStore();

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Stores")
	int32 GetRemainingStores() const { return RemainingStores; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Stores")
	bool CanRelease() const;

	/* Reload on the ground. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Stores")
	void Rearm();

protected:

	virtual void BeginPlay() override;

	/* Resolves the controller that should get credit for what the store does. */
	AController* ResolveInstigatorController() const;

	UPROPERTY(Replicated)
	int32 RemainingStores = 0;

	float LastReleaseTime = -BIG_NUMBER;
};
