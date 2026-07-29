//Copyright 2024 H.Kallisto
//1.0.
#pragma once

#include "CoreMinimal.h"
#include "ForgeVehicle.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "RotorComponent.h"
#include "ForgeRotaryWingVehicle.generated.h"

UENUM(BlueprintType)
enum class EForgeRotaryWingReplicationType : uint8
{
	ERT_K2 UMETA(DisplayName = "K2"),
	ERT_DataOnly UMETA(DisplayName = "Data Only"),
	ERT_None UMETA(DisplayName = "None")
};

USTRUCT()
struct FForgeRotaryWingServerState
{
	GENERATED_USTRUCT_BODY()

public:

	FForgeRotaryWingServerState()
	{
		ServerLinearVelocity = FVector::ZeroVector;
		ServerAngularVelocity = FVector::ZeroVector;
		ServerTransform = FTransform::Identity;
		ServerCurrentGearRatio = 0.f;
		ServerPitchInput = 0.f;
		ServerYawInput = 0.f;
		ServerRollInput = 0.f;
		ServerElevatorInput = 0.f;
	}

	UPROPERTY()
		FVector ServerLinearVelocity;

	UPROPERTY()
		FVector ServerAngularVelocity;

	UPROPERTY()
		FTransform ServerTransform;

		float ServerCurrentGearRatio;

	UPROPERTY()
		float ServerPitchInput;

	UPROPERTY()
		float ServerYawInput;

	UPROPERTY()
		float ServerRollInput;

	UPROPERTY()
		float ServerElevatorInput;


};

/**
 * K2-powered rotary-wing (helicopter) vehicle, re-homed onto the Forge Vehicles foundation. It now
 * derives from AForgeVehicle (Arc seat / exit / ignition framework) and implements
 * IForgeVehicleMovementInterface, mapping the shared base's throttle/steering/vertical demands onto
 * the helicopter's cyclic pitch, tail-rotor yaw and collective (elevator) controls.
 */
UCLASS()
class FORGEVEHICLESROTARYWING_API AForgeRotaryWingVehicle : public AForgeVehicle, public IForgeVehicleMovementInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AForgeRotaryWingVehicle();

	//~ Begin IForgeVehicleMovementInterface — maps unified input onto the helicopter controls.
	virtual void SetThrottleInput(float Value) override { SetPitchInput(Value); }     // forward cyclic
	virtual void SetSteeringInput(float Value) override { SetYawInput(Value); }        // tail-rotor yaw
	virtual void SetVerticalInput(float Value) override { SetElevatorInput(Value); }   // collective
	virtual void StartEngine() override { SetControllable(true); bForgeEngineRunning = true; }
	virtual void StopEngine() override { SetControllable(false); bForgeEngineRunning = false; }
	virtual bool IsEngineRunning() const override { return bForgeEngineRunning; }
	//~ End IForgeVehicleMovementInterface

	/* Set by the shared ignition component (via AForgeVehicle) when the engine reaches the On state. */
	UPROPERTY(BlueprintReadOnly, Category = "ForgeRotaryWingVehicle")
	bool bForgeEngineRunning = false;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;



public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		void UpdateDynamicStability(TArray<float> inputs);

	// Sets the pitch input for the helicopter.
	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		void SetPitchInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetPitchInput(float value);

	// Sets the yaw input for the helicopter.
	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		void SetYawInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetYawInput(float value);

	// Sets the roll input for the helicopter.
	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		void SetRollInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetRollInput(float value);

	// Sets the elevator input for the helicopter.
	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		void SetElevatorInput(float value);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetElevatorInput(float value);

	//Allows the input to be enabled/disabled.
	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		void SetControllable(bool inputEnabled);

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetControllable(bool inputEnabled);

	//This can be used to increase the mass used in the base lift calculations. E.g if a vehicle has entered the cargo bay of an aircraft, more lift will be required.
	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		void SetAddedMassAmount(float massAmount);

	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		void SetIsLanding();

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetIsLanding();

	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		void SetIsTakingOff();

	UFUNCTION(Server, Reliable, WithValidation)
		void ServerSetIsTakingOff();

	UFUNCTION(BlueprintPure, Category = "ForgeRotaryWingVehicle")
		float GetVelocityMS();

	UFUNCTION(BlueprintPure, Category = "ForgeRotaryWingVehicle")
		float GetSpeedKPH();

	UFUNCTION(BlueprintPure, Category = "ForgeRotaryWingVehicle")
		float GetSpeedKnots();

	//Returns the drag force in kilograms (kg).
	UFUNCTION(BlueprintPure, Category = "ForgeRotaryWingVehicle")
		float GetDrag();

	//Returns whether the aircraft is in the taking off phase.
	UFUNCTION(BlueprintPure, Category = "ForgeRotaryWingVehicle")
		bool IsTakingOff();

	//Returns whether the aircraft is in the landing phase.
	UFUNCTION(BlueprintPure, Category = "ForgeRotaryWingVehicle")
		bool IsLanding();

	//Returns whether the aircraft has landed.
	UFUNCTION(BlueprintPure, Category = "ForgeRotaryWingVehicle")
		bool GetIsLanded();

	UFUNCTION(BlueprintCallable, Category = "ForgeRotaryWingVehicle")
		float SmoothThrust(float rotorThrust, float newThrust, float speed);



private:

	void UpdatePhysics();

	void ServerStateSync(bool updatePhysics = true);
	void ClientStateSync(bool updatePhysics = true);
	float GetControllable();

	float GameThreadDeltaTime = 0.f;

	// The core component of the helicopter, which defines its physical shape. Changing this mesh can impact the overall appearance and physics behavior of the helicopter.
	// This can be used together with a skeletal mesh. The Core should be set to not visible and hidden in game so that only the collision geometry is used for physics
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Main", meta = (AllowPrivateAccess = "true"))
		UStaticMeshComponent* Core;
		
	// This is a secondary coorinate system which facilitates the movement and stabilizaiton system.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Main", meta = (AllowPrivateAccess = "true"))
		USceneComponent* TransformedAxis;

	int32 StabilityAxisCount = 0;

	TArray<URotorComponent*> Rotors;
	float Mass = 0.f;
	float AddedMass = 0.f;

	//in meters per second
	float Velocity = 0.f;
	float Drag = 0.f;

	bool bLanding = false;

	float DynamicLiftForce = 0.f;
	float DynamicLandingForce = 0.f;
	bool bTakingOff = false;
	bool bLanded = false;
	bool bInputEnabled = true;

	float TakeOffRatio = 1.f;

protected:
	//This is the maximum speed in meters per second.
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Movement", meta = (AllowPrivateAccess = "true"))
		//float MaxSpeed = 100.f;

	// Rate of pitch change for the helicopter. It is how quicky and how much the aircraft 'dips'. Increasing this value can make the helicopter respond more quickly to pitch inputs, while decreasing it can make the pitch changes slower and smoother.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Movement", meta = (AllowPrivateAccess = "true"))
		float PitchRate = 50.f;

	// Rate of yaw change for the helicopter. It is how quicky and how much the aircraft rotates about its centre. Adjusting this value can make the helicopter yaw more responsively or less responsively to yaw inputs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Movement", meta = (AllowPrivateAccess = "true"))
		float YawRate = 150.f;

	// Rate of roll change for the helicopter. Changing this value can impact how quickly the helicopter rolls in response to roll inputs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Movement", meta = (AllowPrivateAccess = "true"))
		float RollRate = 120.f;

	// Rate of elevator change for the helicopter.It is how quicky and how much the aircraft increases or decreases height .Modifying this value can affect the responsiveness of the elevator control.
	//The unit for this value is Newtons [N]
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Movement", meta = (AllowPrivateAccess = "true"))
		float ElevatorRate = 2750.f;

	// The force applied longitudinally (forward and backward) to the helicopter. Adjusting this force can impact the helicopter's acceleration and deceleration behavior.
	//The unit for this value is Newtons [N]
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Movement", meta = (AllowPrivateAccess = "true"))
		float LongitudinalForce = 50000.0f;

	// The lateral force applied to the helicopter for sideways movement. It is how much and how quick the aircraft can strafe. Changing this value can affect the helicopter's ability to strafe or move sideways.
	//The unit for this value is Newtons [N]
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Movement", meta = (AllowPrivateAccess = "true"))
		float LateralForce = 7500.f;

	// The rate at which the aircraft altitude decreases with pitch input. Modifying this rate can affect how quickly the helicopter decends with pitch input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Movement", meta = (AllowPrivateAccess = "true"))
		float PitchDeclineRate = 1500.f;

	//This controls whether lift is produced to overcome weight. This can be disabled if rotor (physics) thrust is primarily used. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Movement", meta = (AllowPrivateAccess = "true"))
		bool bUseLift = true;

	// The rotational damping for stability. Increasing this value can make the helicopter more stable, while decreasing it can make it less stable, possibly leading to over-rotation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Stability", meta = (AllowPrivateAccess = "true"))
		float RotationalDamping = 1.5f;

	// The linear damping for stability. Adjusting this value can influence how quickly the helicopter comes to a stop when no inputs are applied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Stability", meta = (AllowPrivateAccess = "true"))
		float LinearDamping = 0.1f;

	//If this is disabled the aircraft will not be subject to restorative forces and move freely.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Stability", meta = (AllowPrivateAccess = "true"))
		bool bUseTransformedAxisStability = true;

	// How quickly the system applies stabilization. Higher values may produce more movement but stabilize quicker. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Stability", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseTransformedAxisStability==true", EditConditionHides))
		float StabilizationSpeed = 10.f;

	// The pitch threshold for stability. This is the maximum approximate pitch angle that is possible with full pitch input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Stability", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseTransformedAxisStability==true", EditConditionHides))
		float PitchThreshold = 25.0f;

	// The roll threshold for stability. Modifying this threshold can affect the angle at which the helicopter is considered stable or unstable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Stability", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseTransformedAxisStability==true", EditConditionHides))
		float RollThreshold = 25.0f;

	// The slip correction factor. Changing this value can impact the helicopter's ability to correct for slipping or sliding behavior when moving through air.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Stability", meta = (AllowPrivateAccess = "true"))
		float SlipCorrection = 25000.f;

	// This is a dimensionless scalar. Higher values result in more drag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		float DragCoefficent = 0.3f;

	//Frontal cross sectional area. In meters squared. m^2
	//This is the area that drag acts on. Higher values will result in more drag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		float DragArea = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Aerodynamics", meta = (AllowPrivateAccess = "true"))
		float AirDensity = 1.225f;

	//This is whether the aircraft should speed up to takeoff (true), or is initally in the take off phase (false).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Takeoff and Landing", meta = (AllowPrivateAccess = "true"))
		bool bInitalTakeoff = false;

	//This is how quickly the aircraft will transition to take-off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Takeoff and Landing", meta = (AllowPrivateAccess = "true"))
		float TakeOffSpeed = 25000.f;

	//This is how quickly the aircraft will land.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Takeoff and Landing", meta = (AllowPrivateAccess = "true"))
		float LandingSpeed = 15000.f;

	float PitchInput = 0.f;
	float RollInput = 0.f;
	float YawInput = 0.f;
	float ElevationInput = 0.f;


	//This is how replication is implemented.
	// K2 - Server biased replication
	// Data Only - Movement is not replicated. Only data is replicated. This is useful if a 3rd party movement replication system is used (like 'Smooth Sync').
	// None - No data or movement is replicated. This is useful if you are implementing a custom replication system or do not need replication.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeRotaryWingVehicle|Replication", meta = (AllowPrivateAccess = "true"))
		EForgeRotaryWingReplicationType ReplicationMethod = EForgeRotaryWingReplicationType::ERT_None;

	UPROPERTY(Replicated)
		FForgeRotaryWingServerState ServerState;
};
