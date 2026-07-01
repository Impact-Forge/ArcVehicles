// Copyright Impact-Forge. Gunner seat pawn, reformatted for Forge from the BurningLands ABLVehicleGunner.

#pragma once

#include "CoreMinimal.h"
#include "Seats/ForgeVehicleSeat.h"
#include "ForgeVehicleGunner.generated.h"

class UInputAction;
struct FInputActionValue;

/**
 * Pawn class for a gunner / turret seat on a Forge vehicle.
 *
 * A gunner is a seat pawn (AForgeVehicleSeat) that the occupying player possesses so they can aim
 * independently of the driver: it drives its own camera from the control rotation and feeds mouse
 * look into controller yaw/pitch, which a turret movement component on the vehicle can follow.
 */
UCLASS(NotPlaceable, meta = (DisplayName = "Vehicle Gunner", ScriptName = "VehicleGunner"))
class FORGEVEHICLESCORE_API AForgeVehicleGunner : public AForgeVehicleSeat
{
	GENERATED_BODY()

public:

	AForgeVehicleGunner();

	//~ Begin AActor interface
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual FVector GetPawnViewLocation() const override;

#if WITH_EDITOR
	virtual bool IsSelectable() const override;
#endif // WITH_EDITOR
	// End AActor interface

	//~ Begin APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	// End APawn interface

	//~ Begin AForgeVehiclePawn interface
	virtual void BecomePossessedByPlayer(APlayerState* InPlayerState) override;
	// End AForgeVehiclePawn interface

	/* Returns the character pawn seated in this gunner seat (the player-controlled soldier), if any. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Forge Vehicle|Gunner")
	APawn* GetSeatedPawn() const;

protected:

	UFUNCTION()
	void Input_MouseLook(const FInputActionValue& Value);

	/* Field of view used by the gunner camera. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Forge Vehicle|Gunner")
	float CameraFOV = 90.f;

	/* Enhanced-Input action driving turret look. Bind a 2D (mouse) action. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Forge Vehicle|Gunner")
	TObjectPtr<UInputAction> LookAction;
};
