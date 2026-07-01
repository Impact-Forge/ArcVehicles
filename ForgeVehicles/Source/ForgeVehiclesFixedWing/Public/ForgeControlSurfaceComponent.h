// K2 FixedWing Physics

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "ForgeAeroEngineComponent.h" // for EForgeFixedWingAxis

#include "ForgeControlSurfaceComponent.generated.h"

// Which pilot input deflects this surface.
UENUM(BlueprintType)
enum class EForgeFixedWingControlSurface : uint8
{
	Elevator UMETA(DisplayName = "Elevator (Pitch)"),
	AileronLeft UMETA(DisplayName = "Aileron Left (Roll)"),
	AileronRight UMETA(DisplayName = "Aileron Right (Roll)"),
	Rudder UMETA(DisplayName = "Rudder (Yaw)"),
	Flap UMETA(DisplayName = "Flap")
};

/**
 * Purely cosmetic animated control surface (aileron, elevator, rudder or flap).
 *
 * It produces no physics; it just deflects the attached mesh to match the pilot input so the aircraft
 * reads correctly. The actual aerodynamic response lives on the AForgeFixedWingVehicle body. Pivot the mesh's
 * origin on its hinge line and choose the matching DeflectionAxis.
 */
UCLASS(ClassGroup = (K2FixedWing), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESFIXEDWING_API UForgeControlSurfaceComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	UForgeControlSurfaceComponent();

	// Deflects the surface toward the position commanded by the relevant input. Called each tick by the owner.
	void UpdateSurface(float DeltaTime, float PitchInput, float RollInput, float YawInput, float FlapsInput);

protected:

	virtual void BeginPlay() override;

	// Which control this surface follows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_ControlSurface", meta = (AllowPrivateAccess = "true"))
		EForgeFixedWingControlSurface SurfaceType = EForgeFixedWingControlSurface::Elevator;

	// Local axis the surface hinges about.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_ControlSurface", meta = (AllowPrivateAccess = "true"))
		EForgeFixedWingAxis DeflectionAxis = EForgeFixedWingAxis::Y;

	// Maximum deflection in degrees at full input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_ControlSurface", meta = (AllowPrivateAccess = "true"))
		float MaxDeflection = 20.f;

	// How quickly the surface moves toward its commanded position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_ControlSurface", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
		float DeflectionSpeed = 8.f;

	// Reverse the deflection direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_ControlSurface", meta = (AllowPrivateAccess = "true"))
		bool bInvert = false;

private:

	float CurrentDeflection = 0.f;
	FQuat BaseRelativeRotation = FQuat::Identity;
};
