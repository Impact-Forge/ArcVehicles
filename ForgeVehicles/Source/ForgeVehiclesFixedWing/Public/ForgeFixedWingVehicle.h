// K2 FixedWing Physics

#pragma once

#include "CoreMinimal.h"
#include "ForgeVehicle.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "ForgeAeroEngineComponent.h"
#include "ForgeControlSurfaceComponent.h"
#include "ForgeFixedWingVehicle.generated.h"

// How state is replicated across the network.
// K2        - Server-biased: the server simulates and replicates the resulting physics state to clients.
// Data Only - Only the input/telemetry data is replicated (use with a 3rd-party movement sync).
// None      - Nothing is replicated (single player, or you provide your own replication).
UENUM(BlueprintType)
enum class EForgeFixedWingReplicationType : uint8
{
	ERT_K2 UMETA(DisplayName = "K2"),
	ERT_DataOnly UMETA(DisplayName = "Data Only"),
	ERT_None UMETA(DisplayName = "None")
};

USTRUCT()
struct FForgeFixedWingServerState
{
	GENERATED_USTRUCT_BODY()

public:

	FForgeFixedWingServerState()
	{
		ServerLinearVelocity = FVector::ZeroVector;
		ServerAngularVelocity = FVector::ZeroVector;
		ServerTransform = FTransform::Identity;
		ServerThrottle = 0.f;
		ServerPitchInput = 0.f;
		ServerYawInput = 0.f;
		ServerRollInput = 0.f;
		ServerFlapsInput = 0.f;
		ServerTiltAlpha = 0.f;
		ServerTiltTarget = 0.f;
	}

	UPROPERTY()
		FVector ServerLinearVelocity;

	UPROPERTY()
		FVector ServerAngularVelocity;

	UPROPERTY()
		FTransform ServerTransform;

	UPROPERTY()
		float ServerThrottle;

	UPROPERTY()
		float ServerPitchInput;

	UPROPERTY()
		float ServerYawInput;

	UPROPERTY()
		float ServerRollInput;

	UPROPERTY()
		float ServerFlapsInput;

	UPROPERTY()
		float ServerTiltAlpha;

	UPROPERTY()
		float ServerTiltTarget;
};

/**
 * Fixed-wing aircraft pawn with optional VTOL / tiltrotor capability.
 *
 * The body (Core) simulates physics with gravity. The aircraft must build airspeed for the wings to
 * generate lift (real fixed-wing behaviour); propulsion comes from one or more UForgeAeroEngineComponent
 * children so single-engine, multi-engine and asymmetric layouts all work. For a tiltrotor (Osprey
 * CV-22 style) mark the rotors bCanTilt and drive TiltTarget 0..1 to transition between forward flight
 * and hover; the wing lift and tilted thrust hand off automatically as speed changes.
 */
UCLASS()
class FORGEVEHICLESFIXEDWING_API AForgeFixedWingVehicle : public AForgeVehicle, public IForgeVehicleMovementInterface
{
	GENERATED_BODY()

public:
	AForgeFixedWingVehicle();

	//~ Begin IForgeVehicleMovementInterface — SetThrottleInput is satisfied by the class' own throttle
	// control; steering banks the aircraft and vertical drives the elevator.
	virtual void SetSteeringInput(float Value) override { SetRollInput(Value); }   // aileron / bank
	virtual void SetVerticalInput(float Value) override { SetPitchInput(Value); }  // elevator / pitch
	virtual void StartEngine() override { bForgeEngineRunning = true; }
	virtual void StopEngine() override { bForgeEngineRunning = false; SetThrottleInput(0.f); }
	virtual bool IsEngineRunning() const override { return bForgeEngineRunning; }
	//~ End IForgeVehicleMovementInterface

	/* Set by the shared ignition component (via AForgeVehicle) when the engine reaches the On state. */
	UPROPERTY(BlueprintReadOnly, Category = "ForgeFixedWingVehicle")
	bool bForgeEngineRunning = false;

protected:
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ---- Inputs -----------------------------------------------------------------------------------

	// Engine throttle, 0..1. Drives forward thrust in airplane mode and vertical thrust in hover mode.
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle")
		void SetThrottleInput(float value);
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetThrottleInput(float value);

	// Elevator / pitch, -1..1.
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle")
		void SetPitchInput(float value);
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetPitchInput(float value);

	// Rudder / yaw, -1..1.
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle")
		void SetYawInput(float value);
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetYawInput(float value);

	// Aileron / roll, -1..1.
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle")
		void SetRollInput(float value);
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetRollInput(float value);

	// Flaps, 0..1. Adds lift and drag for takeoff / landing.
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle")
		void SetFlapsInput(float value);
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetFlapsInput(float value);

	// Enable / disable pilot input.
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle")
		void SetControllable(bool inputEnabled);
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetControllable(bool inputEnabled);

	// Increase the effective mass used for hover lift (e.g. cargo loaded). In kilograms.
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle")
		void SetAddedMassAmount(float massAmount);

	// ---- VTOL / Tiltrotor -------------------------------------------------------------------------

	// Set the tilt target directly. 0 = airplane mode (thrust forward), 1 = hover mode (thrust up).
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle|VTOL")
		void SetTiltTarget(float value);
	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetTiltTarget(float value);

	// Convenience: true = go to hover mode, false = go to airplane mode.
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle|VTOL")
		void SetVTOLMode(bool bHover);

	// Convenience: flip between hover and airplane mode.
	UFUNCTION(BlueprintCallable, Category = "ForgeFixedWingVehicle|VTOL")
		void ToggleVTOLMode();

	// ---- Telemetry --------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetForwardSpeedMS();
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetAirspeedMS();
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetSpeedKPH();
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetSpeedKnots();
	// Mach number (total airspeed / speed of sound).
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetMachNumber();
	// Altitude in metres above the configured sea level.
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetAltitudeMeters();
	// The air density (kg/m^3) used last frame (varies with altitude when bUseAltitudeDensity is on).
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetCurrentAirDensity();
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetAngleOfAttack();
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetSideSlip();
	// Lift produced last frame, in kilograms-force (kgf).
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetLift();
	// Drag produced last frame, in kilograms-force (kgf).
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetDrag();
	// Total engine thrust produced last frame, in Newtons.
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetThrust();
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		bool IsStalled();
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle")
		float GetThrottlePercent();
	// Current tilt: 0 = airplane mode, 1 = hover mode.
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle|VTOL")
		float GetTiltAlpha();
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle|VTOL")
		bool IsVTOLEquipped();
	// Throttle (0..1) at which total engine thrust equals weight - i.e. the hover point.
	UFUNCTION(BlueprintPure, Category = "ForgeFixedWingVehicle|VTOL")
		float GetHoverThrottle();

private:

	void UpdatePhysics(float DeltaTime);
	void ServerStateSync(bool updatePhysics = true);
	void ClientStateSync(bool updatePhysics = true);
	float GetControllableScalar() const;

	float GameThreadDeltaTime = 0.f;

	// The collision / physics body. Use a hidden simple-collision mesh and add your visual meshes,
	// engines and control surfaces as children.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Main", meta = (AllowPrivateAccess = "true"))
		UStaticMeshComponent* Core;

	TArray<UForgeAeroEngineComponent*> Engines;
	TArray<UForgeControlSurfaceComponent*> ControlSurfaces;

	float Mass = 0.f;
	float AddedMass = 0.f;

	// Cached telemetry.
	float ForwardSpeed = 0.f;   // m/s, along the nose.
	float Airspeed = 0.f;       // m/s, total.
	float AngleOfAttack = 0.f;  // degrees.
	float SideSlip = 0.f;       // degrees.
	float LiftForce = 0.f;      // Newtons.
	float DragForce = 0.f;      // Newtons.
	float ThrustForce = 0.f;    // Newtons (sum of engines).
	float CurrentAirDensity = 1.225f; // kg/m^3, after altitude adjustment.
	float MachNumber = 0.f;
	float AltitudeMeters = 0.f;
	bool bStalled = false;

	float TiltAlpha = 0.f;      // current tilt (interpolated).

protected:

	/**
	 * The physics body, for subclasses that need to configure it before BeginPlay reads its mass.
	 * Small airframes in particular must set an explicit mass override: a light UAV's auto-computed
	 * mass from collision volume is never the intended value.
	 */
	UStaticMeshComponent* GetCore() const { return Core; }

	// ---- Control authority ------------------------------------------------------------------------

	// Pitch control power (deg/sec^2 of angular acceleration at full input and full authority).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Controls", meta = (AllowPrivateAccess = "true"))
		float PitchRate = 60.f;

	// Yaw control power. Fixed-wing aircraft yaw slowly via the rudder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Controls", meta = (AllowPrivateAccess = "true"))
		float YawRate = 25.f;

	// Roll control power. Fixed-wing aircraft roll briskly via the ailerons.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Controls", meta = (AllowPrivateAccess = "true"))
		float RollRate = 120.f;

	// Forward airspeed (m/s) at which the control surfaces reach full effectiveness. Below this, controls
	// get progressively mushy (no airflow = no authority), which is the classic fixed-wing feel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Controls", meta = (AllowPrivateAccess = "true"))
		float ControlAuthoritySpeed = 60.f;

	// A floor on control authority so the aircraft is never completely uncontrollable (e.g. nose-wheel
	// steering while taxiing). 0 = surfaces are useless at a standstill.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Controls", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
		float MinControlAuthority = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Controls", meta = (AllowPrivateAccess = "true"))
		bool bInvertPitch = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Controls", meta = (AllowPrivateAccess = "true"))
		bool bInvertRoll = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Controls", meta = (AllowPrivateAccess = "true"))
		bool bInvertYaw = false;

	// ---- Aerodynamics -----------------------------------------------------------------------------

	// Master switch for wing lift. Disable only if you want pure thrust-supported flight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		bool bUseLift = true;

	// Total wing reference area in square metres (m^2). Bigger wing = more lift and more drag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		float WingArea = 24.f;

	// Lift coefficient at zero angle of attack (camber). Typical 0.0 - 0.3.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		float BaseLiftCoefficient = 0.2f;

	// Extra lift coefficient per degree of angle of attack (lift-curve slope). Typical ~0.1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		float LiftCurveSlope = 0.1f;

	// Angle of attack (degrees) at which the wing stalls and lift starts to collapse.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		float StallAngle = 15.f;

	// How many degrees past the stall angle lift takes to mostly disappear. Larger = gentler stall.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
		float StallSharpness = 10.f;

	// Parasitic (form) drag coefficient - drag that is always present. Typical 0.02 - 0.05.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		float ParasiticDrag = 0.025f;

	// Wing aspect ratio (span^2 / area). Higher = more efficient (less induced drag). Gliders ~15, fighters ~3.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
		float AspectRatio = 7.f;

	// Oswald efficiency factor for induced drag. Typical 0.7 - 0.85.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", ClampMax = "1.0"))
		float OswaldEfficiency = 0.8f;

	// Air density at sea level (kg/m^3).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		float AirDensity = 1.225f;

	// Thin the air with altitude so lift, drag and (air-breathing) thrust fall off as you climb - this is
	// what gives an aircraft a natural service ceiling. Important for high-flying bombers and fighters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		bool bUseAltitudeDensity = false;

	// World Z (cm) that counts as sea level (0 m altitude) for the density calculation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true", EditCondition = "bUseAltitudeDensity", EditConditionHides))
		float SeaLevelZ = 0.f;

	// Atmospheric scale height in metres - the altitude over which density drops to ~37%. Earth ~8500 m.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", EditCondition = "bUseAltitudeDensity", EditConditionHides))
		float DensityScaleHeight = 8500.f;

	// ---- Compressibility (transonic / supersonic) ------------------------------------------------

	// Add a transonic drag rise around Mach 1 (the "sound barrier"). Relevant to supersonic fighters and
	// bombers - you need extra thrust to punch through, and supercruise sits past the peak.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Compressibility", meta = (AllowPrivateAccess = "true"))
		bool bUseMachDrag = false;

	// Speed of sound in m/s (sea level ~340; it falls with altitude in reality).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Compressibility", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", EditCondition = "bUseMachDrag", EditConditionHides))
		float SpeedOfSound = 340.f;

	// Peak extra drag multiplier at Mach 1 (e.g. 2.0 = up to 3x drag at the barrier).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Compressibility", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", EditCondition = "bUseMachDrag", EditConditionHides))
		float MachDragPeak = 2.f;

	// Width of the transonic drag bump in Mach (smaller = sharper barrier).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Compressibility", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", EditCondition = "bUseMachDrag", EditConditionHides))
		float MachDragWidth = 0.12f;

	// ---- Flaps ------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Flaps", meta = (AllowPrivateAccess = "true"))
		bool bUseFlaps = true;

	// Extra lift coefficient added at full flaps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Flaps", meta = (AllowPrivateAccess = "true", EditCondition = "bUseFlaps", EditConditionHides))
		float FlapsLiftCoefficient = 0.5f;

	// Extra drag coefficient added at full flaps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Flaps", meta = (AllowPrivateAccess = "true", EditCondition = "bUseFlaps", EditConditionHides))
		float FlapsDragCoefficient = 0.06f;

	// ---- Stability --------------------------------------------------------------------------------

	// Angular damping applied to the body (resists rotation). Higher = steadier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Stability", meta = (AllowPrivateAccess = "true"))
		float RotationalDamping = 0.75f;

	// Linear damping applied to the body. Keep small; aerodynamic drag handles most deceleration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Stability", meta = (AllowPrivateAccess = "true"))
		float LinearDamping = 0.f;

	// Natural tendency to point the nose into the airflow (longitudinal + directional static stability).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Stability", meta = (AllowPrivateAccess = "true"))
		bool bUseAerodynamicStability = true;

	// Strength of the nose-into-wind pitch restoring moment (weathervane in pitch, reduces angle of attack).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Stability", meta = (AllowPrivateAccess = "true", EditCondition = "bUseAerodynamicStability", EditConditionHides))
		float PitchStability = 2.f;

	// Strength of the nose-into-wind yaw restoring moment (weathervane in yaw, reduces sideslip).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Stability", meta = (AllowPrivateAccess = "true", EditCondition = "bUseAerodynamicStability", EditConditionHides))
		float YawStability = 2.f;

	// Roll-due-to-sideslip (dihedral effect): rolls wings level when slipping. 0 to disable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Stability", meta = (AllowPrivateAccess = "true", EditCondition = "bUseAerodynamicStability", EditConditionHides))
		float DihedralEffect = 0.5f;

	// Sideways aerodynamic force that resists slipping through the air (keeps the aircraft tracking forward).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Stability", meta = (AllowPrivateAccess = "true"))
		float SlipResistance = 5000.f;

	// Gently return the wings (and pitch) to level when the stick is released. Arcade aid; fades with input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Stability", meta = (AllowPrivateAccess = "true"))
		bool bAutoLevel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Stability", meta = (AllowPrivateAccess = "true", EditCondition = "bAutoLevel", EditConditionHides))
		float AutoLevelStrength = 3.f;

	// ---- VTOL / Tiltrotor -------------------------------------------------------------------------

	// Master switch for VTOL behaviour. Enable for tiltrotors / lift-fan craft.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|VTOL", meta = (AllowPrivateAccess = "true"))
		bool bVTOLCapable = false;

	// Tilt the aircraft starts in. 0 = airplane mode, 1 = hover mode.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|VTOL", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bVTOLCapable", EditConditionHides))
		float InitialTiltAlpha = 1.f;

	// How quickly the nacelles swing between airplane and hover mode (interp speed).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|VTOL", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", EditCondition = "bVTOLCapable", EditConditionHides))
		float TiltRate = 0.6f;

	// Stabilise attitude and damp drift while in hover mode, so the craft is controllable at low speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|VTOL", meta = (AllowPrivateAccess = "true", EditCondition = "bVTOLCapable", EditConditionHides))
		bool bHoverStabilization = true;

	// Strength of the hover attitude-levelling moment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|VTOL", meta = (AllowPrivateAccess = "true", EditCondition = "bVTOLCapable && bHoverStabilization", EditConditionHides))
		float HoverLevelStrength = 6.f;

	// Damping of sideways / vertical drift while hovering (helps the craft hold position).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|VTOL", meta = (AllowPrivateAccess = "true", EditCondition = "bVTOLCapable && bHoverStabilization", EditConditionHides))
		float HoverDriftDamping = 2000.f;

	// Optional arcade aid: cancel gravity while in hover mode so the throttle controls climb/descent
	// around a stationary hover. Leave off for honest thrust-vs-weight hovering.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|VTOL", meta = (AllowPrivateAccess = "true", EditCondition = "bVTOLCapable", EditConditionHides))
		bool bHoverGravityAssist = false;

	// ---- Inputs (state) ---------------------------------------------------------------------------

	float ThrottleInput = 0.f;
	float PitchInput = 0.f;
	float RollInput = 0.f;
	float YawInput = 0.f;
	float FlapsInput = 0.f;
	float TiltTarget = 0.f;
	bool bInputEnabled = true;

	// ---- Replication ------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeFixedWingVehicle|Replication", meta = (AllowPrivateAccess = "true"))
		EForgeFixedWingReplicationType ReplicationMethod = EForgeFixedWingReplicationType::ERT_None;

	UPROPERTY(Replicated)
		FForgeFixedWingServerState ServerState;
};
