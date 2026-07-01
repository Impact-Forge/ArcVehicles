// K2 FixedWing Physics

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"

#include "ForgeAeroEngineComponent.generated.h"

// Principal local axis used for thrust direction or visual spin.
UENUM(BlueprintType)
enum class EForgeFixedWingAxis : uint8
{
	X UMETA(DisplayName = "X (Forward)"),
	Y UMETA(DisplayName = "Y (Right)"),
	Z UMETA(DisplayName = "Z (Up)")
};

// The kind of powerplant. This drives sensible defaults and the feel of spool-up / thrust behaviour.
// Propeller - Responds quickly to throttle. Loses some thrust at high airspeed (optional falloff).
// Jet       - Spools up / down slowly (turbine inertia). Holds thrust well at speed.
// Rotor     - Helicopter-style rotor used for tiltrotor / VTOL lift. Strong static thrust, quick response.
UENUM(BlueprintType)
enum class EForgeFixedWingEngineType : uint8
{
	Propeller UMETA(DisplayName = "Propeller"),
	Jet UMETA(DisplayName = "Jet"),
	Rotor UMETA(DisplayName = "Rotor (VTOL)")
};

// How the engine behaves at runtime.
// Animation Only        - No thrust is produced. The mesh only spins / tilts (purely cosmetic).
// Physics Only          - Thrust is produced at the engine location. The mesh is not spun.
// Physics and Animation - Thrust is produced and the mesh spins / tilts.
UENUM(BlueprintType)
enum class EForgeFixedWingEngineMode : uint8
{
	AnimationOnly UMETA(DisplayName = "Animation Only"),
	PhysicsOnly UMETA(DisplayName = "Physics Only"),
	PhysicsWithAnimation UMETA(DisplayName = "Physics and Animation")
};

/**
 * A single powerplant for a K2 FixedWing aircraft.
 *
 * Attach one component per engine (or rotor) to the aircraft. Each engine produces thrust at its
 * own location, so multi-engine layouts, asymmetric thrust and differential-thrust steering all work
 * naturally. Set bCanTilt on the rotors / props of a tiltrotor (Osprey-style) craft so they swing
 * between forward thrust (airplane mode) and vertical thrust (hover mode) as the aircraft transitions.
 */
UCLASS(ClassGroup = (K2FixedWing), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESFIXEDWING_API UForgeAeroEngineComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	UForgeAeroEngineComponent();

	// Drives the engine for one frame: spools the throttle, applies tilt / vector / spin and produces thrust.
	// Called by the owning AForgeFixedWingVehicle each tick. DensityRatio scales air-breathing thrust with altitude.
	void UpdateEngine(float DeltaTime, UStaticMeshComponent* Body, float Throttle, float PitchInput, float RollInput, float YawInput, float Airspeed, float TiltAlpha, float DensityRatio = 1.f);

	// Current spooled throttle (0..1) after response lag. Useful for audio / FX.
	UFUNCTION(BlueprintPure, Category = "K2_Engine")
		float GetCurrentThrottle() const { return CurrentThrottle; }

	// The thrust produced last frame, in Newtons.
	UFUNCTION(BlueprintPure, Category = "K2_Engine")
		float GetThrustNewtons() const { return LastThrust; }

	// The maximum thrust this engine can produce, in Newtons.
	UFUNCTION(BlueprintPure, Category = "K2_Engine")
		float GetMaxThrust() const { return MaxThrust; }

	// Whether this engine currently contributes thrust.
	UFUNCTION(BlueprintPure, Category = "K2_Engine")
		bool IsEngineEnabled() const { return bEngineEnabled; }

	// Turn the engine on / off at runtime (e.g. engine-out failures).
	UFUNCTION(BlueprintCallable, Category = "K2_Engine")
		void SetEngineEnabled(bool bEnabled) { bEngineEnabled = bEnabled; }

protected:

	virtual void BeginPlay() override;

	// ---- Main -------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Main", meta = (AllowPrivateAccess = "true"))
		EForgeFixedWingEngineMode Mode = EForgeFixedWingEngineMode::PhysicsWithAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Main", meta = (AllowPrivateAccess = "true"))
		EForgeFixedWingEngineType EngineType = EForgeFixedWingEngineType::Propeller;

	// If false the engine produces no thrust (engine-out). Can be toggled at runtime.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Main", meta = (AllowPrivateAccess = "true"))
		bool bEngineEnabled = true;

	// ---- Thrust -----------------------------------------------------------------------------------

	// Maximum thrust produced at full throttle, in Newtons [N]. Tune this against the aircraft mass:
	// total max thrust must comfortably exceed weight (mass * 9.81) for a VTOL/hover craft, and exceed
	// cruise drag for a conventional fixed-wing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float MaxThrust = 20000.f;

	// Fraction of MaxThrust still produced at zero throttle (engine idle). 0 = no idle thrust.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float IdleThrustRatio = 0.f;

	// Local axis the thrust is produced along. X (forward) is normal for props/jets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		EForgeFixedWingAxis ThrustAxis = EForgeFixedWingAxis::X;

	// Reverse the thrust direction along the thrust axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		bool bInvertThrust = false;

	// How quickly the engine spools toward the commanded throttle. Higher = snappier (props), lower = laggy (jets).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float ThrottleResponse = 4.f;

	// If enabled the engine loses thrust as airspeed rises (typical of fixed-pitch propellers).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		bool bThrustFalloff = false;

	// Airspeed (m/s) at which the thrust falloff reaches its maximum reduction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", EditCondition = "bThrustFalloff && Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float FalloffSpeed = 200.f;

	// How much thrust is lost at FalloffSpeed (0 = none, 1 = all).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bThrustFalloff && Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float FalloffAmount = 0.5f;

	// How much the thrust scales with ambient air density (only matters when the aircraft uses altitude
	// density). 0 = unaffected (rocket motor), 1 = fully proportional (air-breathing jet loses thrust with
	// altitude, giving a natural service ceiling). Set ~0.6-0.8 for realistic jets / props.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float ThrustDensityScale = 0.f;

	// ---- Differential control (multi-engine) ------------------------------------------------------
	// These let an engine respond to the pilot's pitch/roll/yaw by trimming its own throttle, which is how
	// you get differential thrust on multi-engine and VTOL craft. Leave at 0 for engines that should not
	// participate. Example (twin layout): left engine YawMix = +0.3, right engine YawMix = -0.3.

	// Constant throttle scale for this engine (e.g. 1.0 normal, 0.0 to feather).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Differential", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float ThrottleMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Differential", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float PitchMix = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Differential", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float RollMix = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Differential", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float YawMix = 0.f;

	// ---- Tilt (VTOL / tiltrotor) ------------------------------------------------------------------

	// Enable for nacelles / rotors that swing between forward thrust and vertical thrust. The aircraft's
	// TiltAlpha (0 = airplane mode, 1 = hover mode) drives the tilt. Because thrust follows the mesh
	// orientation, tilting the nacelle automatically redirects the thrust vector.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Tilt", meta = (AllowPrivateAccess = "true"))
		bool bCanTilt = false;

	// Local axis the nacelle rotates about when tilting. Y (right) swings the forward axis up into hover.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Tilt", meta = (AllowPrivateAccess = "true", EditCondition = "bCanTilt", EditConditionHides))
		EForgeFixedWingAxis TiltAxis = EForgeFixedWingAxis::Y;

	// Angle (degrees) the nacelle rotates through between airplane mode and full hover mode.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Tilt", meta = (AllowPrivateAccess = "true", EditCondition = "bCanTilt", EditConditionHides))
		float MaxTiltAngle = 90.f;

	// Reverse the tilt direction (if the nacelle swings the wrong way).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Tilt", meta = (AllowPrivateAccess = "true", EditCondition = "bCanTilt", EditConditionHides))
		bool bInvertTilt = false;

	// ---- Thrust Vectoring (gen-5 fighters) --------------------------------------------------------
	// Deflects the nozzle in response to pilot input. Because the (deflected) thrust is applied at the
	// engine location, this produces a control moment about the CG that does NOT fade at low airspeed -
	// the key to high-angle-of-attack control on thrust-vectoring fighters (F-22 = pitch, Su-57 = pitch+yaw).
	// For roll on a twin, set RollVectorMix opposite on each engine (differential pitch vectoring).

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust Vectoring", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		bool bThrustVectoring = false;

	// Maximum nozzle deflection in degrees. Real 2D/3D nozzles are ~15-20 degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust Vectoring", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "45.0", EditCondition = "bThrustVectoring && Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float MaxVectorAngle = 17.f;

	// How much pitch input deflects the nozzle (1 = full).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust Vectoring", meta = (AllowPrivateAccess = "true", EditCondition = "bThrustVectoring && Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float PitchVectorMix = 1.f;

	// How much yaw input deflects the nozzle (3D nozzles only; set 0 for pitch-only 2D nozzles).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust Vectoring", meta = (AllowPrivateAccess = "true", EditCondition = "bThrustVectoring && Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float YawVectorMix = 0.f;

	// How much roll input deflects the nozzle in pitch (use opposite signs per side on a twin for roll).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Thrust Vectoring", meta = (AllowPrivateAccess = "true", EditCondition = "bThrustVectoring && Mode!=EForgeFixedWingEngineMode::AnimationOnly", EditConditionHides))
		float RollVectorMix = 0.f;

	// ---- Animation (prop / rotor spin) ------------------------------------------------------------

	// Local axis the visible mesh spins about. X (forward) is normal for a propeller / jet fan.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Animation", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::PhysicsOnly", EditConditionHides))
		EForgeFixedWingAxis SpinAxis = EForgeFixedWingAxis::X;

	// Visual spin speed (deg/sec) at idle / zero throttle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Animation", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::PhysicsOnly", EditConditionHides))
		float MinSpinSpeed = 200.f;

	// Visual spin speed (deg/sec) at full throttle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Animation", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::PhysicsOnly", EditConditionHides))
		float MaxSpinSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Animation", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::PhysicsOnly", EditConditionHides))
		float SpinMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Animation", meta = (AllowPrivateAccess = "true", EditCondition = "Mode!=EForgeFixedWingEngineMode::PhysicsOnly", EditConditionHides))
		bool bInvertSpin = false;

	// ---- Debug ------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Debug", meta = (AllowPrivateAccess = "true"))
		bool bShowDebugThrust = false;

	// Downscale of the debug line so it fits on screen (roughly half the force magnitude used).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Debug", meta = (AllowPrivateAccess = "true", EditCondition = "bShowDebugThrust", EditConditionHides))
		float DebugScale = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "K2_Engine|Debug", meta = (AllowPrivateAccess = "true", EditCondition = "bShowDebugThrust", EditConditionHides))
		float DebugLineThickness = 5.f;

private:

	// Returns the world-space unit vector for a given local axis (reads current component orientation,
	// so it already reflects any tilt that has been applied this frame).
	FVector AxisVector(EForgeFixedWingAxis Axis) const;

	// Spooled throttle (0..1).
	float CurrentThrottle = 0.f;

	// Thrust produced last frame in Newtons.
	float LastThrust = 0.f;

	// Accumulated spin angle (degrees) for the visual rotation.
	float SpinAngle = 0.f;

	// The component's authored relative rotation, captured at BeginPlay. Tilt + spin are composed on top of this.
	FQuat BaseRelativeRotation = FQuat::Identity;
};
