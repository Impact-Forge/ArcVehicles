// Copyright Impact-Forge. Multirotor flight model (quadcopters and larger rotor counts).

#pragma once

#include "CoreMinimal.h"
#include "ForgeVehicle.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "Math/ForgeDroneMath.h"
#include "Math/ForgePID.h"

#include "ForgeMultirotorVehicle.generated.h"

class URotorComponent;
class UStaticMeshComponent;

/** How the vehicle synchronises state, mirroring the other Forge flight modules. */
UENUM(BlueprintType)
enum class EForgeMultirotorReplicationType : uint8
{
	/* Authority simulates and pushes state; clients follow. */
	ERT_Full UMETA(DisplayName = "Full"),
	/* Authority simulates; only control inputs are synchronised (for third-party smoothing). */
	ERT_DataOnly UMETA(DisplayName = "Data Only"),
	/* Every machine simulates locally. Suitable for single-player and prototyping. */
	ERT_None UMETA(DisplayName = "None")
};

/** Stabilisation behaviour, i.e. what the sticks actually command. */
UENUM(BlueprintType)
enum class EForgeMultirotorFlightMode : uint8
{
	/* Sticks command a bank/pitch angle and release returns to level. Camera drones. */
	Angle UMETA(DisplayName = "Angle (self-levelling)"),
	/* Sticks command rotation rates with no levelling - full inversion, FPV feel. */
	Acro UMETA(DisplayName = "Acro (rate)")
};

/** One motor: where it sits, which way it spins, and how hard it can push. */
USTRUCT(BlueprintType)
struct FORGEVEHICLESDRONES_API FForgeMultirotorMotor
{
	GENERATED_BODY()

	/* Motor position relative to the airframe origin, centimetres. +X forward, +Y right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motor")
	FVector RelativeLocationCm = FVector::ZeroVector;

	/* Propeller direction. Alternate around the airframe so reaction torques cancel in hover. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motor")
	bool bClockwise = false;

	/* Peak static thrust, newtons. Four of these against mass sets the thrust-to-weight ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motor", meta = (ClampMin = "0.0"))
	float MaxThrustN = 8.f;

	/* Optional rotor component name driven as this motor's propeller visual. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motor")
	FName RotorComponentName = NAME_None;

	/* Cleared when the motor is destroyed or burns out. */
	UPROPERTY(BlueprintReadOnly, Category = "Motor")
	bool bEnabled = true;

	/* Last commanded output, [0, 1]. Drives propeller spin rate and audio. */
	UPROPERTY(BlueprintReadOnly, Category = "Motor")
	float Output = 0.f;
};

/** Replicated flight state. */
USTRUCT()
struct FForgeMultirotorServerState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 ServerLinearVelocity = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize10 ServerAngularVelocity = FVector::ZeroVector;

	UPROPERTY()
	FTransform ServerTransform = FTransform::Identity;

	UPROPERTY()
	float ServerThrottle = 0.f;

	UPROPERTY()
	float ServerPitch = 0.f;

	UPROPERTY()
	float ServerRoll = 0.f;

	UPROPERTY()
	float ServerYaw = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeMotorFailed, int32, MotorIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeFlightModeChanged, EForgeMultirotorFlightMode, NewMode);

/**
 * Multirotor airframe: any number of fixed, upward-facing motors mixed to produce thrust and
 * attitude, as quadcopters and their larger relatives actually fly.
 *
 * This is a separate flight model from AForgeRotaryWingVehicle on purpose. That class approximates a
 * helicopter as one body with scripted lift and a separate horizontal shove, so tilting it does not
 * actually move it. A multirotor's translation *is* its tilt: thrust is applied per motor at each
 * motor's own location, so leaning the airframe redirects the whole thrust vector and the aircraft
 * accelerates for the same physical reason a real one does.
 *
 * Physics runs on the authority (or everywhere in ERT_None); the numeric core lives in
 * ForgeDrone::Multirotor so it can be tested without a world.
 */
UCLASS()
class FORGEVEHICLESDRONES_API AForgeMultirotorVehicle : public AForgeVehicle, public IForgeVehicleMovementInterface
{
	GENERATED_BODY()

public:

	AForgeMultirotorVehicle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin AActor / APawn interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	// End AActor / APawn interface

	//~ Begin IForgeVehicleMovementInterface
	/* Forward/back stick. In Angle mode this is a pitch angle, in Acro a pitch rate. */
	virtual void SetThrottleInput(float Value) override { SetPitchStick(Value); }
	virtual void SetSteeringInput(float Value) override { SetYawStick(Value); }
	/* Collective: climb demand. Maps to the throttle channel of the mixer. */
	virtual void SetVerticalInput(float Value) override { SetThrottleStick(Value); }
	virtual void SetRollAxisInput(float Value) override { SetRollStick(Value); }
	virtual void SetYawAxisInput(float Value) override { SetYawStick(Value); }
	virtual void StartEngine() override;
	virtual void StopEngine() override;
	virtual bool IsEngineRunning() const override { return bMotorsArmed; }
	// End IForgeVehicleMovementInterface

	//~ Control inputs, all normalised [-1, 1] except throttle which is [0, 1] in Angle mode.
	UFUNCTION(BlueprintCallable, Category = "ForgeMultirotor|Input")
	void SetThrottleStick(float Value);

	UFUNCTION(BlueprintCallable, Category = "ForgeMultirotor|Input")
	void SetPitchStick(float Value);

	UFUNCTION(BlueprintCallable, Category = "ForgeMultirotor|Input")
	void SetRollStick(float Value);

	UFUNCTION(BlueprintCallable, Category = "ForgeMultirotor|Input")
	void SetYawStick(float Value);

	UFUNCTION(BlueprintCallable, Category = "ForgeMultirotor|Flight")
	void SetFlightMode(EForgeMultirotorFlightMode NewMode);

	UFUNCTION(BlueprintPure, Category = "ForgeMultirotor|Flight")
	EForgeMultirotorFlightMode GetFlightMode() const { return FlightMode; }

	/* Disable or restore a motor (battle damage, failure). Server authoritative. */
	UFUNCTION(BlueprintCallable, Category = "ForgeMultirotor|Flight")
	void SetMotorEnabled(int32 MotorIndex, bool bEnabled);

	/* Throttle fraction that holds a hover at the current mass and remaining motors. */
	UFUNCTION(BlueprintPure, Category = "ForgeMultirotor|Flight")
	float GetHoverThrottle() const;

	UFUNCTION(BlueprintPure, Category = "ForgeMultirotor|Flight")
	float GetThrustToWeightRatio() const;

	/* Mean of the current motor outputs, [0, 1]. Battery draw and audio read this. */
	UFUNCTION(BlueprintPure, Category = "ForgeMultirotor|Flight")
	float GetMeanMotorOutput() const;

	UFUNCTION(BlueprintPure, Category = "ForgeMultirotor|Flight")
	const TArray<FForgeMultirotorMotor>& GetMotors() const { return Motors; }

	/**
	 * Fraction of rated thrust the motors can currently produce, [0, 1]. Systems that sap power
	 * (a flat battery, a damaged ESC) drive this; zero means the aircraft falls.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeMultirotor|Flight")
	void SetPowerScale(float NewPowerScale);

	UFUNCTION(BlueprintPure, Category = "ForgeMultirotor|Flight")
	float GetPowerScale() const { return PowerScale; }

	UPROPERTY(BlueprintAssignable, Category = "ForgeMultirotor|Flight")
	FOnForgeMotorFailed OnMotorFailed;

	UPROPERTY(BlueprintAssignable, Category = "ForgeMultirotor|Flight")
	FOnForgeFlightModeChanged OnFlightModeChanged;

protected:

	/* Physics body. The inherited skeletal Mesh is cosmetic and re-parented under this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Core;

	//~ Airframe -------------------------------------------------------------

	/* Motor layout. Left empty, an X-configuration quad is generated from ArmLengthCm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Airframe")
	TArray<FForgeMultirotorMotor> Motors;

	/* Half the motor-to-motor diagonal, centimetres. Used when auto-generating a quad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Airframe", meta = (ClampMin = "1.0"))
	float ArmLengthCm = 12.f;

	/* Peak thrust per motor when auto-generating, newtons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Airframe", meta = (ClampMin = "0.1"))
	float DefaultMotorThrustN = 8.f;

	//~ Flight ---------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight")
	EForgeMultirotorFlightMode FlightMode = EForgeMultirotorFlightMode::Angle;

	/* How much of the throttle range each attitude axis may claim in the mixer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight")
	float PitchAuthority = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight")
	float RollAuthority = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight")
	float YawAuthority = 0.25f;

	/* Maximum commanded lean in Angle mode, degrees. Higher means faster but twitchier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight|Angle", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float MaxTiltAngleDeg = 35.f;

	/* Outer loop gain converting attitude error (deg) into a rate target (deg/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight|Angle", meta = (ClampMin = "0.1"))
	float AngleModeLevelGain = 6.f;

	/* Climb rate commanded by a full throttle stick in Angle mode, m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight|Angle", meta = (ClampMin = "0.1"))
	float ClimbRateMS = 5.f;

	/* Maximum rotation rates in Acro mode, degrees/second (pitch, roll, yaw). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight|Acro")
	FVector AcroMaxRatesDps = FVector(720.f, 720.f, 400.f);

	/* Inner rate loops: rotation-rate error to mixer command. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight|Tuning")
	FForgePIDController PitchRatePID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight|Tuning")
	FForgePIDController RollRatePID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight|Tuning")
	FForgePIDController YawRatePID;

	/* Climb-rate loop used in Angle mode, around the hover-throttle feed-forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Flight|Tuning")
	FForgePIDController ClimbRatePID;

	//~ Aerodynamics ---------------------------------------------------------

	/* Airframe drag coefficient; also how strongly wind pushes the aircraft around. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Aero", meta = (ClampMin = "0.0"))
	float BodyDragCoefficient = 1.1f;

	/* Reference frontal area, m^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Aero", meta = (ClampMin = "0.0"))
	float FrontalAreaM2 = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Aero", meta = (ClampMin = "0.0"))
	float AirDensity = 1.225f;

	/* Angular damping applied to the body, standing in for rotor gyroscopic damping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Aero", meta = (ClampMin = "0.0"))
	float RotationalDamping = 0.35f;

	//~ Networking -----------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeMultirotor|Networking")
	EForgeMultirotorReplicationType ReplicationMethod = EForgeMultirotorReplicationType::ERT_Full;

	UPROPERTY(Replicated)
	FForgeMultirotorServerState ServerState;

	UPROPERTY(Replicated)
	bool bMotorsArmed = false;

	/* Motor enable flags, replicated so clients can stop the propeller visual of a dead motor. */
	UPROPERTY(ReplicatedUsing = OnRep_MotorEnabledMask)
	int32 MotorEnabledMask = MAX_int32;

	UFUNCTION()
	void OnRep_MotorEnabledMask();

	//~ Optional extra input actions layered on top of the shared vehicle bindings.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeMultirotor|Input")
	TObjectPtr<UInputAction> RollAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForgeMultirotor|Input")
	TObjectPtr<UInputAction> FlightModeToggleAction;

	void Input_Roll(const struct FInputActionValue& Value);
	void Input_ToggleFlightMode(const struct FInputActionValue& Value);

	//~ Server RPCs, matching the per-axis idiom used by the other flight modules.
	UFUNCTION(Server, Unreliable)
	void ServerSetSticks(float InThrottle, float InPitch, float InRoll, float InYaw);

	UFUNCTION(Server, Reliable)
	void ServerSetFlightMode(EForgeMultirotorFlightMode NewMode);

	//~ Simulation -----------------------------------------------------------

	/* Runs the control loops and applies motor forces. Authority (or ERT_None) only. */
	virtual void UpdatePhysics(float DeltaTime);

	/* Converts sticks into normalised attitude commands for the mixer. */
	void ComputeAttitudeCommands(float DeltaTime, float& OutPitchCommand, float& OutRollCommand, float& OutYawCommand);

	/* Converts the throttle stick into a collective demand. Non-const: the climb loop carries state. */
	float ComputeCollective(float DeltaTime);

	void ApplyMotorForces();
	void ApplyAerodynamicDrag(float DeltaTime);
	void UpdateRotorVisuals();

	/* Builds a default X-quad when no motors are authored. */
	void EnsureMotorLayout();

	/* Caches the rotor components named by the motor layout. */
	void CacheRotorComponents();

	void ServerStateSync();
	void ClientStateSync();

	/* Snapshot of the motor geometry handed to the pure mixer. */
	void BuildMotorGeometry(TArray<ForgeDrone::Multirotor::FMotorGeometry>& OutGeometry) const;

	//~ Runtime state --------------------------------------------------------

	float ThrottleStick = 0.f;
	float PitchStick = 0.f;
	float RollStick = 0.f;
	float YawStick = 0.f;

	float PowerScale = 1.f;
	float MassKg = 1.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URotorComponent>> CachedRotors;

	TArray<float> MotorOutputs;
};
