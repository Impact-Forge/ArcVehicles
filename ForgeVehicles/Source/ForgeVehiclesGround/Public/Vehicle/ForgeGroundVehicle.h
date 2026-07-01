//Copyright 2024 P.Kallisto 

#pragma once
#include "Vehicle/SuspensionComponent.h"
#include "Vehicle/ForgeGroundInteriorComponent.h"
#include "Vehicle/ArticulatedComponent.h"
#include "Vehicle/StaticVehicleWheel.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Components/SplineComponent.h" 
#include "CoreMinimal.h"
#include "Vehicle/ForgeGroundVehicleWheel.h"
#include "AsyncTickPawn.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "ForgeGroundVehicle.generated.h"


UENUM(BlueprintType)
enum class ESpeedClamp : uint8
{
	NoClamp UMETA(Display = "No Clamp"),
	Constant UMETA(Display = "Constant"),
	Linear UMETA(Display = "Linear"),
	Curve UMETA(Display = "Curve")
};

UENUM(BlueprintType)
enum class EBehaviourType : uint8
{
	Standard UMETA(Display = "Standard"),
	Arcade UMETA(Display = "Arcade")
};


UENUM(BlueprintType)
enum class ETorqueImplementation : uint8
{
	ETI_Constant UMETA(DisplayName = "Constant"),
	ETI_Curve UMETA(DisplayName = "Curve")
};

UENUM(BlueprintType)
enum class EReplicationType : uint8
{
	ERT_Full UMETA(DisplayName = "Full"),
	ERT_DataOnly UMETA(DisplayName = "Data Only"),
	ERT_None UMETA(DisplayName = "None")
};


USTRUCT(BlueprintType)
struct FRangedClamp
{
	GENERATED_USTRUCT_BODY()

public:

	FRangedClamp()
	{
		InputRange = FVector2D::ZeroVector;
		OutputRange = FVector2D::ZeroVector;
	}

	UPROPERTY(EditAnywhere, Category = "Custom Data")
	FVector2D InputRange;

	UPROPERTY(EditAnywhere, Category = "Custom Data")
	FVector2D OutputRange;

};

USTRUCT()
struct FServerState
{
	GENERATED_USTRUCT_BODY()

public:

	FServerState()
	{
		ServerLinearVelocity = FVector::ZeroVector;
		ServerAngularVelocity = FVector::ZeroVector;
		ServerTransform = FTransform::Identity;
		ServerRPM = 0.f;
		ServerCurrentGear = 0.f;
		ServerCurrentGearRatio = 0.f;
		ServerThrottle = 0.f;
		ServerSteering = 0.f;
		ServerBrakes = 0.f;
		ServerSpeed = 0.f;
		ServerIsUpshifting = false;
		ServerIsDownshifting = false;
	}

	UPROPERTY()
		FVector ServerLinearVelocity;

	UPROPERTY()
		FVector ServerAngularVelocity;

	UPROPERTY()
		FTransform ServerTransform;
		
	UPROPERTY()
		float ServerRPM;

	UPROPERTY()
		float ServerCurrentGear;

	UPROPERTY()
		float ServerCurrentGearRatio;

	UPROPERTY()
		float ServerThrottle;

	UPROPERTY()
		float ServerSteering;

	UPROPERTY()
		float ServerBrakes;

	UPROPERTY()
		float ServerSpeed;

	UPROPERTY()
		bool ServerIsUpshifting;

	UPROPERTY()
		bool ServerIsDownshifting;



	//int UpshiftFrameCount = 0;
	//int DownshiftFrameCount = 0;


};


/**
 * RTune-powered ground vehicle, re-homed onto the Forge Vehicles foundation. Via AAsyncTickPawn it
 * now derives from AForgeVehicle, so it gains the Arc seat / exit / ignition framework for free, and
 * it implements IForgeVehicleMovementInterface so the shared base can route throttle/steering input
 * and ignition into RTune's existing SetThrottleInput / SetSteeringInput control surface.
 */
UCLASS()
class FORGEVEHICLESGROUND_API AForgeGroundVehicle : public AAsyncTickPawn, public IForgeVehicleMovementInterface
{
	GENERATED_BODY()

public:

	AForgeGroundVehicle();

	//~ Begin IForgeVehicleMovementInterface (throttle/steering are satisfied by RTune's own
	// SetThrottleInput / SetSteeringInput below; these gate power on engine ignition).
	virtual void StartEngine() override { bForgeEngineRunning = true; }
	virtual void StopEngine() override { bForgeEngineRunning = false; SetThrottleInput(0.f); }
	virtual bool IsEngineRunning() const override { return bForgeEngineRunning; }
	//~ End IForgeVehicleMovementInterface

	/* Set by the shared ignition component (via AForgeVehicle) when the engine reaches the On state. */
	UPROPERTY(BlueprintReadOnly, Category = "ForgeGroundVehicle")
	bool bForgeEngineRunning = false;

	virtual void NativeAsyncTick(float DeltaTime) override;

	void UpdatePhysicsCore(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void AsyncReplicatedTick();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;


	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void InitializeAnimationSystem();

	//Simulates animation. This can be used when using the take recorder for in-editor playback.
	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SimulateInternalAnimation();

	//Handles internal animation of the wheels. This can be used when using the take recorder.
	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
	void SimulateInternalWheelAnimation();


	//Initializes the anti gravity system. This is called once in Begin Play.
	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void InitializeAntiGravity(USceneComponent* antiGravityComponent, UStaticMeshComponent* coreMesh);

	//Initializes the lean steer system. This is called once in Begin Play. A rear suspension component should be used. This is not strictly required and any suspension component can be used.
	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void InitializeLeanSteer(USuspensionComponent* suspensionComponent);


	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetThrottleInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetThrottleInput(float value);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void ApplyHandbrakeInput(bool value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerApplyHandbrakeInput(bool value);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetBrakeInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetBrakeInput(float value);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetSteeringInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetSteeringInput(float value);

	//Allows the input to be enabled/disabled. Useful when unpossessing the vehicle to prevent continual input feed.
	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetControllable(bool inputEnabled);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetControllable(bool inputEnabled);

	// Values for the amount that are around the same as the mass generally work well.
	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void Jump(float amount);


	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetAirPitchInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetAirPitchInput(float value);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetAirYawInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetAirYawInput(float value);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetAirRollInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetAirRollInput(float value);

#pragma region ReplicationInput

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle|Replication|Input")
		void SetExtReplicationInput1(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetExtReplicationInput1(float value);

	float ExtReplicationInput1 = 0.f;

	UFUNCTION(BlueprintPure, Category = "Replication")
		float GetExtReplicationInput1();

	///

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle|Replication|Input")
		void SetExtReplicationInput2(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetExtReplicationInput2(float value);

	float ExtReplicationInput2 = 0.f;

	UFUNCTION(BlueprintPure, Category = "Replication")
		float GetExtReplicationInput2();

	///

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle|Replication|Input")
		void SetExtReplicationInput3(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetExtReplicationInput3(float value);

	float ExtReplicationInput3 = 0.f;

	UFUNCTION(BlueprintPure, Category = "Replication")
		float GetExtReplicationInput3();

	///

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle|Replication|Input")
		void SetExtReplicationInput4(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetExtReplicationInput4(float value);

	float ExtReplicationInput4 = 0.f;

	UFUNCTION(BlueprintPure, Category = "Replication")
		float GetExtReplicationInput4();

	///

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle|Replication|Input")
		void SetExtReplicationInput5(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetExtReplicationInput5(float value);

	float ExtReplicationInput5 = 0.f;

	UFUNCTION(BlueprintPure, Category = "Replication")
		float GetExtReplicationInput5();

	///

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle|Replication|Input")
		void SetExtReplicationInput6(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetExtReplicationInput6(float value);

	float ExtReplicationInput6 = 0.f;

	UFUNCTION(BlueprintPure, Category = "Replication")
		float GetExtReplicationInput6();



#pragma endregion
	//UFUNCTION(Server, Reliable, WithValidation)
		//void ServerJump(float amount);

#pragma region BlueprintCallableSettors

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetEngineTorque(float newTorque);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetCurrentGearRatio(float newGearRatio);

	//This function allows for a specific gear to be selected. Please note, if a non-existent gear is specified, the function will not work.
	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
	    void SetGear(int32 newGear);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetDifferentialRatio(float newDifferentialRatio);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetPhysicsWheelRadius(float newRadius);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetExternalResistanceForce(float force);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetBrakesEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetEngineTorqueEnabled(bool bEnabled, float tempTorque);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void SetInAir(bool bAirborne);

	void Shift(bool bUpshift);

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void Upshift();

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void Downshift();

	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void OverrideRPM(float newRPM);
	
	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void OverrideAntiGravityState(bool bOn);

	//Resume physics wake state
	UFUNCTION(BlueprintCallable, Category = "ForgeGroundVehicle")
		void RTWakeRigidBodies();


	void SetArcadeMaxSpeed(float newMaxSpeed);

#pragma endregion

#pragma region BlueprintCallableGettors

	//Returns the speed of the vehicle in kilometers per hour.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetSpeedKPH();

	//Returns the local velocity in the forward direction of the vehicle in meters per second (m/s).
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetLinearVelocity();

	//Returns the local velocity vector in meters per second (m/s).
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		FVector GetVelocityVector();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetThrottleInput();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetBrakesInput();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetSteeringInput();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		bool GetHandbrakeInput();

	// Returns the peak engine torque. To get the current engine torque use 'GetEngineCurrentTorque'.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetEngineTorque();

	// Returns the current engine torque.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetEngineCurrentTorque();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetCurrentGearRatio();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetDifferentialRatio();

	// Returns the wheel radius of the drivetrain.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetPhysicsWheelRadius();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetExternalResistanceForce();

	// Returns the current RPM.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetRPM();

	// Returns whether or not the gearbox is upshifting.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		bool IsGearboxUpshifting();

	// Returns whether or not the gearbox is downshifting.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		bool IsGearboxDownshifting();

	// Returns the current gear index.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		int GetCurrentGear();

	//Returns the async tick physics transform of the vehicle.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		FTransform GetPhysicsTransform();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		bool GetIsHandbrakeActive();

	//Returns the angle between the vehicle's forward direction and velocity angle, in degrees. This is calculated on the physics (async) thread and should be used only in Event Async Tick.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetSideSlipAngle();

	//Returns the angle between the vehicle's forward direction and velocity angle, in degrees. This is calculated on the game thread and should be used only in Event Tick.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetSideSlipAngleGT();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetHandbrakeStrength();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		UPrimitiveComponent* GetPrimitiveRoot();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		bool IsAntiGravityActive();

	//Returns the G's (m/s^2) in the forward direction.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetLongitudinalG();

	//Returns the G's (m/s^2) in the sideways direction.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetLateralG();

	//Returns the G's (m/s^2) in the upwards direction.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetVerticalG();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		FVector GetLocationVelocity();

	//Returns the averaged normalized slip. 
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetAverageDrivetrainSlip();

	//Returns drivetrain slip gamma.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetAveragedSlipGamma();

	//Returns whether traction is broken.
	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		bool IsSlipping();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetAngularVelocity();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetAngularAcceleration();

	UFUNCTION(BlueprintPure, Category = "ForgeGroundVehicle")
		float GetDrivetrainWheelCount();

	TArray<USuspensionComponent*> GetSuspension();

#pragma endregion

	UPROPERTY(Replicated)
		FServerState ServerState;

	UPROPERTY(Replicated)
		FVector ServerVelocity;

	UPROPERTY(Replicated)
		FVector ServerAngularVelocity;


protected:

#pragma region Powertrain

	// Specifies the powertrain behaviour.
	// Arcade: Torque is constant with no gearbox in place.
	// Standard: Full powertrain with variable torque as a function of RPM.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Engine", meta = (AllowPrivateAccess = "true"))
	EBehaviourType PowertrainType = EBehaviourType::Arcade;

	//In Newton-Meters. This is the maximum torque that the engine will produce. If the standard powertrain is used, the peak value on the curve will be this value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Engine", meta = (AllowPrivateAccess = "true"))
	float EngineTorque = 500.f;

	//Arcade maximum speed clamp. In KPH. This is an approximate top speed, the actual top speed will be slightly lower than this value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Engine", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Arcade", EditConditionHides))
	float MaxSpeed = 318.f;

	// Maximum RPM, engine RPM will not exceed this value unless an override is applied during runtime. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Engine", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	float MaxRPM = 7000.f;

	//This can be used to scale RPM. Be very cautious when modifying this as it can cause erratic behaviour, but in some applications be be useful. The default value of 1 is perfectly stable and will not have any impact.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Engine", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	float RPM_Multiplier = 1.f;

	// How torque is implemented. Either it is a constant value, i.e. acceleration through a gear will not change, or it is a function of RPM. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Engine", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	ETorqueImplementation TorqueImplementation = ETorqueImplementation::ETI_Constant;

	// This is a normalized torque curve which should be from 0 to 1. The delivered engine torque will be the value at a point on this curve multiplied by the EngineTorque.
	// X axis: RPM
	// Y axis: Torque multiplier
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Engine", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard &&TorqueImplementation==ETorqueImplementation::ETI_Curve", EditConditionHides))
	UCurveFloat* EngineTorqueCurve;


	//This is how much of the power produced is transmitted to the wheels. This can be reduced to simulate losses.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Engine", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
		float EffectivePower = 1.f;

	// This affects how quickly the RPM decreases when the throttle is no applied. Higher values result in a quicker decrease. Lower values result in less off-throttle deceleration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Engine", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides), meta = (ClampMin = "0"), meta = (ClampMax = "1"))
	float EngineBrakeCoefficient = 0.35f;

	//Effective power will be overriden
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
		bool bUseDrivetrain = false;

	//This can be used to increase how much power is reduced by due to wheel slip. Higher values will result in more power lost.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard&&bUseDrivetrain==true", EditConditionHides))
		float SlipPowerReduction = 1.f;

	//How quickly slip will be initiated when traction is broken.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard&&bUseDrivetrain==true", EditConditionHides))
		float SlipInitiateSpeed = 15.f;

	//How quickly the non-slip state is restored when the throttle is released and torque is no longer applied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard&&bUseDrivetrain==true", EditConditionHides))
		float SlipRestoreSpeed = 0.5f;

	//How quickly the RPM will increase when the wheels are in the air, and no resistance friction is applied by the road.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard&&bUseDrivetrain==true", EditConditionHides))
		float InAirSpeedUp = 10.f;

	//How quickly the RPM will increase when the wheels are in the air, and no resistance friction is applied by the road.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard&&bUseDrivetrain==true", EditConditionHides))
		float InAirSlowdownSpeed = 2.f;

		float AveragedSlip = 1.f;
		float AveragedSlipGamma = 0.f;
		float DrivetrainWheelCount = 0.f;

	// A simple scalar that affects only acceleration at any given gear. Higher values result in a higher efficiency which increases acceleration. This does not affect top speed. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (ClampMin = "0"), meta = (ClampMax = "1"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	float TransmissionEfficiency = 0.7f;

	//How long do shifts (upshift/downshift) take in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	float ShiftDelay = 0.2f;

	//This is the the ratio between the number of revolutions the drive wheels make and the number of revolutions the engine makes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	float DifferentialRatio = 3.05f;

	//A gear ratio, single element, is the the ratio between the number of revolutions the gear makes and the number of revolutions the engine makes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	TArray<float> GearRatios;

	// This is independant from the actual wheel radius in the suspension component. This is used in the wheel torque calculation and is in meters (m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	float WheelRadius = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	bool bAutomatic = true;

	//Instead of using the RPM to determine when the gearbox upshifts, or downshifts, use the vehicle speed to determine the shift conditions. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	bool bUseSpeedBasedShifting = false;

	//This controls the speed (in KPH) at which the gearbox will upshift and downshift. The X value is the lower bound shift ratio (point at which downshifting will occur), the Y value is the upper bound shift ratio (point at which an upshift will occur)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard && bUseSpeedBasedShifting==true", EditConditionHides))
	TArray<FVector2D> SpeedShiftingRatios;

	// Ratio threshold of RPM to MaxRPM for an upshift to take place. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bAutomatic==true&&PowertrainType==EBehaviourType::Standard && bUseSpeedBasedShifting==false", EditConditionHides), meta = (ClampMin = "0"), meta = (ClampMax = "1"))
	float UpshiftRatio = 0.85f;

	// Ratio threshold of RPM to MaxRPM for an downshift to take place. Gear ratios should be spaced out such that an instance of intersection between an upshift and downshift ratio occur. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bAutomatic==true&&PowertrainType==EBehaviourType::Standard && bUseSpeedBasedShifting==false", EditConditionHides), meta = (ClampMin = "0"), meta = (ClampMax = "1"))
	float DownshiftRatio = 0.3f;

	//This prevents the gearbox from shifting when set to true. This is useful under certain conditions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Transmission", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "PowertrainType==EBehaviourType::Standard", EditConditionHides))
	bool bLockShift = false;

	// Brake force in Newtons. If UseBrakesAsReverse is used, then this will be the force used to reverse.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Brakes", meta = (AllowPrivateAccess = "true"))
	float BrakeForce = 15000.f;

	// Default for arcade powertrain
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Brakes", meta = (AllowPrivateAccess = "true"))
	bool bUseBrakeAsReverse = true;

	//This is the maximum speed (KPH) that can be acheived in reverse if UsedBrakesAsReverse is used. If not the maximum speed in reverse will be determined by the gear ratio set.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Brakes", meta = (AllowPrivateAccess = "true"))
	float MaxSpeedInReverse = 60.f;

	// This is an assistance/helper torque (in Unreal Torque Units) that is tied to steering input and can be used to increase the rotation rate of the vehicle.
	// It is fully compliant with the rest of the vehicle behaviour and will conform to all of it's other systems.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Handling", meta = (AllowPrivateAccess = "true"))
	float DriftTorque = 0.f;

	// Clamp the steering input as a function of speed. I.E. at higher speeds the maximum steering input will be lower, which is how vehicles generally behave.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Handling", meta = (AllowPrivateAccess = "true"))
	ESpeedClamp SteeringInputClamp = ESpeedClamp::NoClamp;

	// Clamp the max steering input across all speeds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Handling", meta = (AllowPrivateAccess = "true"), meta = (ClampMin = "0"), meta = (ClampMax = "1"), meta = (EditCondition = "SteeringInputClamp==ESpeedClamp::Constant", EditConditionHides))
	float ConstantSteeringClamp = 1.f;

	// Linear mapping: 1-to-1 input to speed decrease. The input range is the speed range Input.X = min speed, Input.Y = max speed. The output range is the maximum steering input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Handling", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "SteeringInputClamp==ESpeedClamp::Linear", EditConditionHides))
	FRangedClamp LinearSteeringInputClamp;

	// Steering input decrease as a function of this curve. The x axis is speed, the y axis is the maximum steering value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Handling", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "SteeringInputClamp==ESpeedClamp::Curve", EditConditionHides))
	UCurveFloat* CurveSteeringInputClamp;

	// Limit the steering sensitivity as a function of speed. I.E at higher speeds the steering response will be slower.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Handling", meta = (AllowPrivateAccess = "true"))
	bool bUseSensitivityClamp = false;

	// Constant sensitivity. Sensitivity will not vary with speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Handling", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseSensitivityClamp==false", EditConditionHides))
	float SteeringSensitivity = 5.f;

	// Linear mapping. Input range is speed, output range is the sensitivity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Handling", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseSensitivityClamp==true", EditConditionHides))
	FRangedClamp SteeringSensitivityClamp;

	//This the handbrake force used to slow down the vehicle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Handbrake", meta = (AllowPrivateAccess = "true"))
	float HandbrakeStrength = 7500.f;

	bool bHandbrakeActive = false;

	// This affects the pitch moment. I.E increasing this value increases the amount the vehicle leans under acceleration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Dynamics", meta = (AllowPrivateAccess = "true"))
	float DynamicPitchMomentMultiplier = 0.f;

	// This affects the pitch moment under braking. I.E increasing this value increases the amount the vehicle leans under braking. This can be problematic in certain cases - caution is advised.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Dynamics", meta = (AllowPrivateAccess = "true"))
	float DynamicBrakeMomentMultiplier = 0.f;

	// This allows the vehicle to lean while steering like a bike. Please note that 'Initialize Lean Steer' needs to be called in Begin Play for this functionality.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Dynamics", meta = (AllowPrivateAccess = "true"))
	bool bUseSteerLean = false;

	// This controls the force applied that causes the vehicle to lean.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Dynamics", meta = (AllowPrivateAccess = "true"))
	float SteerLeanCoefficient = 4.85f;

	//Minimum speed the vehicle needs to acheive for lean steer to activate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Dynamics", meta = (AllowPrivateAccess = "true"))
	float SteerLeanMinSpeed = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Dynamics", meta = (AllowPrivateAccess = "true"))
	FVector2D SteerLeanMapping = FVector2D(0.9f, 0.99f);

	USuspensionComponent* LeanSteerSuspension;

	// This is a dimensionless scalar. Higher values result in more drag. Generally this value ranges from 0.25-0.4 for most road cars.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
	float CoefficientOfDrag = 0.3f;

	//Frontal cross sectional area. In meters squared. m^2
	//This is the area that drag acts on. Higher values will result in more drag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
	float CrossSectionArea = 1.5872454f;

	//This is the atmosphere air density used in drag force calculations. Default value is the air density at sea level. In kg/m^3.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
	float AirDensity = 1.225f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Air", meta = (AllowPrivateAccess = "true"))
	float PitchStrength = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Air", meta = (AllowPrivateAccess = "true"))
	float YawStrength = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Air", meta = (AllowPrivateAccess = "true"))
	float RollStrength = 2.f;

	// This controls the in air condition where forces and torques cannot be applied. This variables controls how many wheels have to be off the ground for the vehicle to be considered airborne.
	// When the vehicle is airborne forces and torques are no longer applied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Air", meta = (AllowPrivateAccess = "true"))
	int32 InAirWheelCount = 2;

	// Enabling this activates the Anti Gravity System.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|AntiGravity", meta = (AllowPrivateAccess = "true"))
	bool bAntiGravityEnabled = false;

	//Actual magnitude of gravity. In cm/s^2
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|AntiGravity", meta = (AllowPrivateAccess = "true"))
	float GravityStrength = 980.f;

	// How much to scale the anti-gravity on the Z axis. Increasing this value will increase the anti gravity surface suction. A value of 1 - 2 generally works well. This can be increased higher.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|AntiGravity", meta = (AllowPrivateAccess = "true"))
	float AntiGravityZStrength = 1.2f;

	// How much to scale the anti-gravity on the planar X axis. A value greater than 1 will prevent rolling backwards on inclines. Set this value to zero to allow for forwards and backwards rolling.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|AntiGravity", meta = (AllowPrivateAccess = "true"))
	float AntiGravityXStrength = 1.02f;

	// How much to scale the anti-gravity on the planar Y axis. A value greater than 1 will prevent sideways slipping on inclines. Set this value to zero to allow for sideways slipping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|AntiGravity", meta = (AllowPrivateAccess = "true"))
	float AntiGravityYStrength = 1.02f;

	//This is the time, in seconds, to keep the Anti Gravity system active when the wheels have lost contact with the surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|AntiGravity", meta = (AllowPrivateAccess = "true"))
	float AntiGravityOffContactThreshold = 1.f;

	float AntiGravityOffContactTime = 0;
	bool bAntiGravityActive = false;

	//The added mass used for the anti gravity simulation to maintain equilibrium. Using multiple components that have masses will add to this simulation
	//This can be used if there is a mass change during runtime.
	//This is a highly efficient mass change implementation and will not impact performance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|AntiGravity", meta = (AllowPrivateAccess = "true"))
	float AddedMass = 0.f;


#pragma region IdleLock

	// This system is used to hold a vehicle in place when stationary or travelling very slowly after a certain amount of time and prevent micro slipping. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|IdleLock", meta = (AllowPrivateAccess = "true"))
	bool bUseIdleLock = true;

	// How long to wait (in seconds) to lock vehicle in place.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|IdleLock", meta = (AllowPrivateAccess = "true"))
	float IdleLockTime = 1.5f;

	// This is effectively an override variable that can be called during runtime to disable the idle lock. This may be useful for edge cases that require more precise control of this system.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|IdleLock", meta = (AllowPrivateAccess = "true"))
	bool bIsIdleLocked = false;

	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeGroundVehicle|IdleLock", meta = (AllowPrivateAccess = "true"))
	float IdleLockCount = 0;

	// Speed at which to begin transitioning to idle locked state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|IdleLock", meta = (AllowPrivateAccess = "true"))
	float IdleLockSpeedThreshold = 10.f;

	// This is the force used on inclines to prevent slipping and micro slipping. If the force is too high it may cause an impulse and move the vehicle out of the idle locked state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|IdleLock", meta = (AllowPrivateAccess = "true"))
	float IdleLockStiffness = 3000.f;

	bool bIsIdleLockedRotationLocation = false;
	float IdleLockRotationLocationCount = 0.f;

	//This is how replication is implemented.
	// Full - Complete replication
	// Data Only - Movement is not replicated. Only data is replicated. This is useful if a 3rd party movement replication system is used (like 'Smooth Sync').
	// None - No data or movement is replicated. This is useful if you are implementing a custom replication system or do not need replication.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeGroundVehicle|Replication", meta = (AllowPrivateAccess = "true"))
	EReplicationType ReplicationMethod = EReplicationType::ERT_None;

	//This periodically forces the server to sync transform data to the client for absolute synchronization.  
	//The is optional and not needed most cases.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeGroundVehicle|Replication", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "ReplicationMethod==EReplicationType::ERT_Full", EditConditionHides))
	bool bUseServerFastSync = true;

	//The sync period running on the game thread (not physics thread). 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeGroundVehicle|Replication", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseServerFastSync==true&&ReplicationMethod==EReplicationType::ERT_Full", EditConditionHides))
	int ServerFastSyncLatency = 500;

	//This controls whether to automatically wake rigid body components. Disabling this option can resolve issues with having lots of replicated vehicles.
	//If disabled use the function 'RTWakeRigidBodies' to resume wake state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Physics|Misc", meta = (AllowPrivateAccess = "true"))
	bool bAutoWakeRigidBodies = true;

	//Enabled the animation system required for take recorder animations. Please do not enable this for gameplay, and only enable it when creating animations.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeGroundVehicle|Ultimate Animation System", meta = (AllowPrivateAccess = "true"))
	bool bEnableAnimationMode = false;

	/*
	* This is a speed multiplier that will be applied to all wheels.
	* When using animation mode the suspension component multiplier will no longer be active.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "ForgeGroundVehicle|Ultimate Animation System", meta = (AllowPrivateAccess = "true"))
	float AnimationGlobalWheelSpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeGroundVehicle|Ultimate Animation System", meta = (AllowPrivateAccess = "true"))
	float AnimDeltaTime = 1.f / 60.f;

private:

	void AnimateSteering();

	float GetProcessedSteeringInput(float value);
	float GetControllableValue();

	void ServerStateSync(bool updatePhysics = true);
	void ClientStateSync(bool updatePhysics = true);


	UWorld* World;

	TArray<USuspensionComponent*> SuspensionComponents;
	TArray<UForgeGroundInteriorComponent*> InteriorComponents;
	TArray<UArticulatedComponent*> ArticulatedComponents;
	TArray<UStaticVehicleWheel*> StaticWheels;
	TArray<UForgeGroundVehicleWheel*> AnimWheels;

	int FastServerSyncCount = 0;

	//UStaticMeshComponent* PrimitiveComponent;
	UPrimitiveComponent* PrimitiveComponent;
	USceneComponent* AntiGravity;
	UStaticMeshComponent* AntiGravityCoreMesh;
	float AntiGravityMass = 0.f;
	bool bOverrideAntiGravity = false;

	FTransform PhysicsTransform;
	FVector PreviousLocalVelocity = FVector::ZeroVector;
	FVector LocalVelocity = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	float AngularVelocity = 0.f;
	float LastAngularVelocity = 0.f;
	float AngularAcceleration = 0.f;

	FTransform TransformGT;
	FVector LocalVelocityGT = FVector::ZeroVector;
	float SideSlipAngleGT = 0.f;

	float Xd = 3.15f;
	float Xg = 3.f;

	float ResultantForce = 0.f;
	float ExternalResistanceForce = 0.f;

	float BrakeForceDuplicate = BrakeForce;
	float EngineTorqueDuplicate = EngineTorque;

	float AsyncDeltaTime = 0.f;
	float GameDeltaTime = 0.f;

	float Drag = 0.f;
	float DownForce = 0.f;

	float RPM = 0.f;
	int CurrentGear = 0;
	float CurrentTorque = 0.f;
	float CurrentGearRatio = 0.f;
	float HandbrakeForce = 0.f;

	//In m/s
	float Speed = 0.f;

#pragma region GearboxDelay
	FTimerHandle ShiftDelayTimerHandle;
	bool ShiftTimerActive = false;
	bool IsUpshifting = false;
	bool IsDownshifting = false;
	int UpshiftFrameCount = 0;
	int DownshiftFrameCount = 0;

	float NETWORK_TEMP_X = 0;
#pragma endregion


#pragma region Auxiliary
	bool bInAir = false;
#pragma endregion

#pragma region Input

	//UPROPERTY(Replicated)
	float Throttle = 0.f;

	//UPROPERTY(Replicated)
	float Brakes = 0.f;

	//UPROPERTY(Replicated)
	float SteeringInput = 0.f;

	bool bHandbrakeInput = false;

	float PitchInput = 0.f;
	float RollInput = 0.f;
	float YawInput = 0.f;

	bool bInputEnabled = true;

#pragma endregion

#pragma endregion

#pragma region AnimXSystem

	FVector CurrentLocation = FVector::ZeroVector;
	FVector PreviousLocation = FVector::ZeroVector;
	float CurrentDelta = 0.f;
	float PreviousDelta = 0.f;
	FVector LocationVelocity = FVector::ZeroVector;

#pragma endregion



	
};
