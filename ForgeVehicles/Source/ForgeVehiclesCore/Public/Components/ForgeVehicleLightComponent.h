// Copyright Impact-Forge. Replicated vehicle light (headlights, tail/brake lights, indicators).

#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"

#include "ForgeVehicleLightComponent.generated.h"

class ULightComponent;
class UMeshComponent;

/** What role a light plays, so abilities and vehicle logic can address lights by function. */
UENUM(BlueprintType)
enum class EForgeVehicleLightType : uint8
{
	Headlight,
	HighBeam,
	TailLight,
	BrakeLight,
	ReverseLight,
	TurnIndicator,
	Interior,
	/* Navigation/anti-collision strobes for aircraft and drones. */
	Navigation
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnForgeVehicleLightChanged, UForgeVehicleLightComponent*, Light, bool, bIsOn);

/**
 * A single addressable light fixture on a vehicle.
 *
 * Attach light sources (and optionally an emissive lens mesh) beneath this component and it will
 * drive them together. On/off state is replicated, so lights are visible to every client rather
 * than only the driver.
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESCORE_API UForgeVehicleLightComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UForgeVehicleLightComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// End UActorComponent interface

	/* Function this fixture serves. Abilities and vehicle code address lights by this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeVehicle|Light")
	EForgeVehicleLightType LightType = EForgeVehicleLightType::Headlight;

	/* Whether the fixture is lit when the vehicle spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeVehicle|Light")
	bool bStartsOn = false;

	/* Intensity applied to the driven lights when on. Negative keeps each light's authored value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeVehicle|Light")
	float OnIntensity = -1.f;

	/**
	 * Optional emissive lens material: a scalar parameter set to 1 when lit and 0 when dark on every
	 * mesh beneath this component. Leave as None to skip material work entirely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeVehicle|Light")
	FName EmissiveParameterName = NAME_None;

	/* Fired on server and clients whenever the fixture changes state. */
	UPROPERTY(BlueprintAssignable, Category = "ForgeVehicle|Light")
	FOnForgeVehicleLightChanged OnLightChanged;

	/* Server-authoritative state change. Call on the server; clients follow via replication. */
	UFUNCTION(BlueprintCallable, Category = "ForgeVehicle|Light")
	void SetLightOn(bool bNewOn);

	UFUNCTION(BlueprintCallable, Category = "ForgeVehicle|Light")
	void ToggleLight() { SetLightOn(!bIsOn); }

	UFUNCTION(BlueprintPure, Category = "ForgeVehicle|Light")
	bool IsLightOn() const { return bIsOn; }

protected:

	UFUNCTION()
	void OnRep_IsOn();

	/* Pushes the current state onto the driven light components and lens materials. */
	void ApplyLightState();

	/* Collects the light and mesh components attached beneath this fixture. */
	void GatherDrivenComponents();

	UPROPERTY(ReplicatedUsing = OnRep_IsOn)
	bool bIsOn = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULightComponent>> DrivenLights;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> DrivenMeshes;

	/* Authored intensities, captured before the first state change so "off" can restore them. */
	TArray<float> DefaultIntensities;
};
