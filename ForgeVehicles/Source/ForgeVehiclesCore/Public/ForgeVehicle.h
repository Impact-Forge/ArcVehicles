// Copyright Impact-Forge. Game-agnostic concrete vehicle base, distilled from the BurningLands ABLVehicle.

#pragma once

#include "CoreMinimal.h"
#include "ForgeBaseVehicle.h"
#include "Components/ForgeEngineIgnitionComponent.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "ForgeVehicleTypes.h"
#include "ForgeVehicle.generated.h"

class USkeletalMeshComponent;
class UForgeVehicleExitPoint;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Concrete, game-agnostic vehicle base for every Forge vehicle type.
 *
 * Sits on top of the Arc-derived AForgeBaseVehicle seat/exit framework and adds the pieces every
 * powered vehicle shares: a skeletal mesh, an occupant exit point, a replicated engine ignition
 * component, Enhanced-Input driven throttle/steer/vertical/engine bindings, and per-seat data.
 *
 * It never talks to a specific physics module. Instead it drives whatever movement solution the
 * concrete subclass exposes through IForgeVehicleMovementInterface (RTune ground physics, K2 rotary
 * and fixed wing physics, or the water-craft buoyancy solution), so the ground / rotary-wing /
 * fixed-wing / water-craft modules only have to supply their propulsion and point the base at it.
 */
UCLASS(Abstract)
class FORGEVEHICLESCORE_API AForgeVehicle : public AForgeBaseVehicle
{
	GENERATED_BODY()

public:

	AForgeVehicle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin AActor interface
	virtual void PostInitializeComponents() override;
	// End AActor interface

	//~ Begin APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	// End APawn interface

	//~ Begin AForgeVehiclePawn interface
	virtual void NotifyPlayerSeatChangeEvent_Implementation(APlayerState* Player, UForgeVehicleSeatConfig* ToSeat, UForgeVehicleSeatConfig* FromSeat, EForgeVehicleSeatChangeType SeatChangeEvent) override;
	// End AForgeVehiclePawn interface

	/* The movement solution this vehicle drives. Default returns this actor if it implements the
	 * movement interface; subclasses that host a movement component should override to return it. */
	UFUNCTION(BlueprintCallable, Category = "ForgeVehicle")
	virtual TScriptInterface<IForgeVehicleMovementInterface> GetVehicleMovementInterface();

	/* Returns the FForgeSeatData for the passed seat if it is a UForgeSeatConfig. */
	UFUNCTION(BlueprintCallable, Category = "ForgeVehicle")
	bool GetSeatData(UForgeVehicleSeatConfig* Seat, FForgeSeatData& OutSeatData) const;

	/* Toggles engine ignition on the driver client (start if off, stop if on). */
	UFUNCTION(BlueprintCallable, Category = "ForgeVehicle|Engine")
	void ToggleEngine();

	UForgeEngineIgnitionComponent* GetIgnitionComponent() const { return IgnitionComponent; }
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

protected:

	/* Bound to IgnitionComponent->OnEngineIgnitionStateChanged: starts/stops the movement solution. */
	UFUNCTION()
	virtual void OnEngineIgnitionStateChanged(UForgeEngineIgnitionComponent* Component, EForgeEngineIgnitionState NewState, EForgeEngineIgnitionState OldState);

	/* Enhanced-Input handlers. */
	virtual void Input_Throttle(const FInputActionValue& Value);
	virtual void Input_Steering(const FInputActionValue& Value);
	virtual void Input_Vertical(const FInputActionValue& Value);
	virtual void Input_ToggleEngine(const FInputActionValue& Value);

	/* Adds/removes the driver mapping context for a locally controlled occupant. */
	void UpdateDriverInputMapping(bool bAdd);

public:

	/* Visual representation and physics body of the vehicle. Subclasses may re-root as needed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* Mesh;

	/* Default point occupants are placed at when they leave the vehicle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UForgeVehicleExitPoint* OccupantExitPoint;

	/* Replicated engine ignition state machine. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UForgeEngineIgnitionComponent* IgnitionComponent;

protected:

	/* Enhanced-Input mapping context added for the driver while they occupy the driver seat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	TObjectPtr<UInputMappingContext> DriverMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	int32 DriverMappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	TObjectPtr<UInputAction> SteeringAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	TObjectPtr<UInputAction> VerticalAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	TObjectPtr<UInputAction> EngineToggleAction;

	/* Whether the throttle holds its value when input stops (helicopters/boats) vs. springs back. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input|Throttle")
	uint32 bIsThrottleCollective : 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input|Throttle")
	float ThrottleInputCoefficient = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input|Steering")
	float SteeringInputCoefficient = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input|Vertical")
	float VerticalInputCoefficient = 1.f;
};
