// Copyright Impact-Forge. Game-agnostic concrete vehicle base, distilled from the BurningLands ABLVehicle.

#pragma once

#include "AbilitySystemInterface.h"
#include "Components/ForgeEngineIgnitionComponent.h"
#include "Components/ForgeVehicleLightComponent.h"
#include "CoreMinimal.h"
#include "ForgeBaseVehicle.h"
#include "ForgeVehicleTypes.h"
#include "Interface/TBIA_Interactable.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "ForgeVehicle.generated.h"

class UAbilitySystemComponent;
class UArcInventoryComponent;
class UForgeVehicleAttributeSet;
class UForgeVehicleExitPoint;
class UForgeVehicleRunOverComponent;
class UGameplayAbility;
class UInputAction;
class UInputMappingContext;
class USkeletalMeshComponent;
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
class FORGEVEHICLESCORE_API AForgeVehicle : public AForgeBaseVehicle, public ITBIA_Interactable, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AForgeVehicle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// End IAbilitySystemInterface

	/* Vehicle condition / fuel / stowed ammunition attributes. */
	UForgeVehicleAttributeSet* GetVehicleAttributes() const { return VehicleAttributes; }

	//~ Vehicle lights
	/* Sets every light fixture of the given type. Server authoritative; state replicates. */
	UFUNCTION(BlueprintCallable, Category = "ForgeVehicle|Lights")
	void SetLightsOfType(EForgeVehicleLightType LightType, bool bOn);

	/* Flips every fixture of the given type, using the first fixture's state as the reference. */
	UFUNCTION(BlueprintCallable, Category = "ForgeVehicle|Lights")
	void ToggleLightsOfType(EForgeVehicleLightType LightType);

	/* True when any fixture of the given type is lit. */
	UFUNCTION(BlueprintPure, Category = "ForgeVehicle|Lights")
	bool AreLightsOfTypeOn(EForgeVehicleLightType LightType) const;

	UFUNCTION(BlueprintPure, Category = "ForgeVehicle|Lights")
	void GetLightsOfType(EForgeVehicleLightType LightType, TArray<UForgeVehicleLightComponent*>& OutLights) const;

	//~ Begin ITBIA_Interactable (Twisted Bytes Interaction System)
	/* Vehicles are interactable by default; override/extend for locked or destroyed states. */
	virtual bool IsAvailableForInteraction_Implementation(const UPrimitiveComponent* InteractedComponent, const AActor* InteractingActor) const override;
	/* On a completed interaction, seat the interacting actor's player in the first open seat. */
	virtual void OnPostInteract_Implementation(const AActor* InteractingActor, const UPrimitiveComponent* InteractedComponent) override;
	//~ End ITBIA_Interactable

	/* Blueprint hook fired after a successful interaction, before the default "enter seat" behaviour. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ForgeVehicle|Interaction")
	void OnVehicleInteracted(AActor* InteractingActor);

	/* Both are optional subobjects and are null on subclasses that suppress them - check before use. */
	UForgeVehicleRunOverComponent* GetRunOverComponent() const { return RunOverComponent; }
	UArcInventoryComponent* GetVehicleInventory() const { return VehicleInventory; }

	//~ Begin AActor interface
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	// End AActor interface

	//~ Begin APawn interface (ability actor info follows possession)
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	// End APawn interface

	//~ Begin APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	/* Tears the driver contexts down when possession is lost, including via a raw UnPossess that
	 * never goes through the seat framework (drones, debug possession, AI hand-off). */
	virtual void NotifyControllerChanged() override;
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

	/**
	 * Adds/removes the driver input contexts (shared vehicle base layer + this vehicle's own layer)
	 * for a locally controlled occupant.
	 *
	 * @param ForController  Controller whose local player owns the contexts. Defaults to the current
	 *                       controller; pass the outgoing controller explicitly when tearing down
	 *                       after possession has already moved away.
	 */
	void UpdateDriverInputMapping(bool bAdd, APlayerController* ForController = nullptr);

	/**
	 * Adds/removes the per-seat input context declared in FForgeSeatData for the occupant of a seat.
	 * No-op for non-local occupants and for seats without a context. Runs on the owning client, which
	 * the seat framework reaches through UForgeVehiclePlayerSeatComponent's OnRep.
	 */
	void UpdateSeatInputMapping(APlayerState* Player, UForgeVehicleSeatConfig* Seat, bool bAdd);

public:

	/* Visual representation and physics body of the vehicle. Subclasses may re-root as needed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* Mesh;

	/* Default point occupants are placed at when they leave the vehicle. Null on uncrewed subclasses. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UForgeVehicleExitPoint* OccupantExitPoint;

	/* Replicated engine ignition state machine. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UForgeEngineIgnitionComponent* IgnitionComponent;

	/* Detects and damages pawns the vehicle runs over. Null on subclasses too light to run anything over. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UForgeVehicleRunOverComponent* RunOverComponent;

	/* Arc Inventory storage carried by the vehicle (cargo / mounted equipment). Null on subclasses with no hold. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArcInventoryComponent* VehicleInventory;

	/**
	 * The vehicle's own ability system. Vehicles own their ASC rather than borrowing an occupant's,
	 * so vehicle state (condition, fuel, lights, damage effects) survives crew changing and works
	 * on unmanned platforms with no occupant at all.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

protected:

	/* Attribute set granted to the vehicle's own ability system. */
	UPROPERTY()
	TObjectPtr<UForgeVehicleAttributeSet> VehicleAttributes;

	/* Abilities granted to the vehicle itself on BeginPlay (lights, horn, self-repair, ...). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/* Points the ability actor info at this vehicle; safe to call repeatedly. */
	void InitializeAbilitySystem();

	/**
	 * Shared "any vehicle" input context (throttle, steer, exit, engine toggle), added underneath the
	 * per-vehicle context below. Set this on a common vehicle base Blueprint so every vehicle inherits
	 * the standard controls, and leave DriverMappingContext for what makes each vehicle different.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	TObjectPtr<UInputMappingContext> VehicleBaseMappingContext;

	/* Priority of the shared layer. Kept below DriverMappingPriority so vehicles can override binds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	int32 VehicleBaseMappingPriority = 0;

	/* Per-vehicle Enhanced-Input context added for the driver while they occupy the driver seat.
	 * Layered on top of VehicleBaseMappingContext; higher priority wins on conflicting binds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	TObjectPtr<UInputMappingContext> DriverMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeVehicle|Input")
	int32 DriverMappingPriority = 1;

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

private:

	/* Controller the driver contexts are currently applied to. NotifyControllerChanged fires after
	 * possession has already moved, so the outgoing controller has to be remembered to clean up. */
	TWeakObjectPtr<APlayerController> DriverInputController;
};
