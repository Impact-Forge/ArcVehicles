// Copyright Impact-Forge. FPV strike quadcopter flown into its target.

#pragma once

#include "CoreMinimal.h"
#include "Flight/ForgeMultirotorVehicle.h"

#include "ForgeFPVKamikazeQuad.generated.h"

class UForgeDroneBatteryComponent;
class UForgeDroneLinkComponent;
class UForgeDroneWarheadComponent;

/**
 * Small first-person-view strike quadcopter: fast, agile, expendable, flown directly into a target.
 *
 * Tuned as the opposite of the reconnaissance quad. It defaults to Acro mode, where the sticks command
 * rotation rates with no self-levelling, because that is what allows the continuous rolls and dives of
 * FPV flying and how these drones are really flown. Thrust-to-weight is high and endurance is short -
 * minutes, not half an hour - so it is a weapon launched at a known target rather than a scout.
 *
 * Its run-over component is deliberately kept: a five-inch quad arriving at forty metres a second is a
 * physical impact, whether or not the warhead functions.
 */
UCLASS()
class FORGEVEHICLESDRONES_API AForgeFPVKamikazeQuad : public AForgeMultirotorVehicle
{
	GENERATED_BODY()

public:

	AForgeFPVKamikazeQuad(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	/* All-up mass including the warhead, kg. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeFPVQuad", meta = (ClampMin = "0.05"))
	float AirframeMassKg = 1.2f;

	UForgeDroneBatteryComponent* GetBattery() const { return Battery; }
	UForgeDroneLinkComponent* GetLink() const { return Link; }
	UForgeDroneWarheadComponent* GetWarhead() const { return Warhead; }

	/* Arm the warhead. Still subject to the warhead's own delay and minimum distance. */
	UFUNCTION(BlueprintCallable, Category = "ForgeFPVQuad")
	void ArmWarhead();

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneBatteryComponent> Battery;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneLinkComponent> Link;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForgeDroneWarheadComponent> Warhead;
};
