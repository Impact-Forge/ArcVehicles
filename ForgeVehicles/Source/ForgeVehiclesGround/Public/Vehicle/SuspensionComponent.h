//Copyright 2025 P.Kallisto 

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CollisionShape.h"
#include "Engine/World.h"

//5.4
#include "Engine/HitResult.h"

#include "SuspensionComponent.generated.h"


UENUM(BlueprintType)
enum class EHandbrakeWheelType : uint8
{
	None UMETA(Display = "None"),
	Front UMETA(Display = "Front"),
	Rear UMETA(Display = "Rear")
};

UCLASS( ClassGroup=(ForgeVehiclesGround), meta=(BlueprintSpawnableComponent) )
class FORGEVEHICLESGROUND_API USuspensionComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USuspensionComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void ConstructWheelMeshComponent();

	//Core update function. This will automatically be called if used inside the ForgeGroundVehicle pawn, however, if this is used inside a pawn not inheriting from this class it needs to be called inside a tick function - preferably an Async based tick.
	UFUNCTION(BlueprintCallable, Category = "SuspensionComponent")
		void UpdatePhysics(float DeltaTime, float transmittedTorque, float slipForward, float slipBackwards, float throttle, float brakes);

	//Returns the static mesh component, not to be confused with a pure static mesh. If the wheel's current static mesh is required, use 'GetStaticMesh' on this return.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		UStaticMeshComponent* GetWheelMeshComponent();

	//Set the stiffness of the suspension.
	UFUNCTION(BlueprintCallable, Category = "SuspensionComponent")
		void SetStiffness(float inStifness);

	//Set the stiffness of the suspension.
	UFUNCTION(BlueprintCallable, Category = "SuspensionComponent")
		void SetDamping(float inDamping);

	//Set the stiffness of the suspension.
	UFUNCTION(BlueprintCallable, Category = "SuspensionComponent")
		void SetExtensionLength(float inLength);

	//To render the wheels in the editor/viewport/blueprint editor, call this function inside the construction script.
	UFUNCTION(BlueprintCallable, Category = "SuspensionComponent Construction", meta = (DeprecatedFunction, DeprecationMessage = "Function has been deprecated. Wheels will be rendered during play"))
		void RenderWheelMesh();

	//Allows for a different physics body to be used when applying forces.
	UFUNCTION(BlueprintCallable, Category = "SuspensionComponent")
		void SetNewPhysicsBody(UPrimitiveComponent* physicsBody);

	//Returns if the suspension component, or more specifically the wheel, is in contact with the surface.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		bool IsInAir();

	//Returns the current length of the suspension. This is the difference between the maximum extension length and the compression distance due to the total weight.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		float GetCurrentLength();

	//Returns the hit result data from the raycast.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		FHitResult GetHitResult();

	//Returns how much compression the spring is undergoing. This is a normalized value between 0 and 1.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		float GetCompression();
	//Returns whether the wheel affects the handbrake.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		bool IsHandbrakeWheel();

	//Returns the steering rotation amount in degrees.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		float GetSteeringRotation();

	//Returns the relative offset in the Z axis used to locate the wheel.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		float GetWheelRelativeOffset();

	//Returns the wheel local rotation used for animating the wheel. Units are Radians per second.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		FRotator GetWheelLocalRotation();

	//Returns whether this is a driven wheel with drivetrain torque applied.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		bool GetIsDrivenWheel();

	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		float GetSlipRatio();	

	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
		float GetSlipGamma();


	//Returns an offset impact point that can be used for the location of tyre smoke/trails.
	UFUNCTION(BlueprintPure, Category = "SuspensionComponent")
	FVector GetOffsetImpactPoint(float zOffset);

	EHandbrakeWheelType GetWheelHandbrake();

	bool IsLeftWheel();
	float GetWheelRadius();
	float GetAngularVelocityMultiplier();
	float GetBurnoutRotation();

	//Must be called inside Tick, not AsyncTick.
	//UFUNCTION(BlueprintCallable, Category = "Suspension|Procedural|Animation")
		//void AnimateWheel(UStaticMeshComponent* MeshComponent);


	//Approximate function for rendering wheel position in editor. Call only in construction script.
	UFUNCTION(BlueprintCallable, Category = "Suspension|Procedural")
		void StaticallyRenderWheel(UStaticMeshComponent* MeshComponent);

	//If 'StaticallyRenderWheel' is used, this must be used. Call only in event begin play.
	UFUNCTION(BlueprintCallable, Category = "Suspension|Procedural")
		void StaticWheelCallback(UStaticMeshComponent* MeshComponent);
		
	//UStaticMesh* ConstructorWheelMesh;
	//Set in statically place wheel
	float WheelStaticRadius = 0.f;

	//Sets the scale of the suspensions wheel mesh component
	UFUNCTION(BlueprintCallable, Category = "SuspensionComponent")
		void SetWheelScale(float inScale);

	//Recalculates the wheel radius during runtime. This is useful if the wheel mesh has been changed to a different sized wheel during runtime.
	UFUNCTION(BlueprintCallable, Category = "SuspensionComponent")
		void RecalculateWheelRadius();

	/*	UFUNCTION(BlueprintPure, Category = "Animation")
		float GetWheelAnimLocation();

	UFUNCTION(BlueprintPure, Category = "Animation")
		FRotator GetAnimationRotator();

*/
	void UpdateTick(float DeltaTime);
	void SetSteeringInput(float inSteering, float sensitivity);


	UFUNCTION(BlueprintCallable, Category = "SuspensionComponent")
		void SimulateInternalAnimation(float deltaTime, UStaticMeshComponent* wheelMeshComponent);

	FVector Previous_UAS_Location = FVector::ZeroVector;
	FVector Current_UAS_Location = FVector::ZeroVector;

	void UpdateAnimationState(bool bInUseAnimationSystem, float inLocationVelocity);
	bool bUseAnimationSystem = false;
	float LocationVelocity = 0.f;

	//UFUNCTION(Client, Reliable)
		//void ClientSetSteeringInput(float inSteering, float sensitivity);

	bool IsSteeringWheel();

	void UpdateSteeringGeometry(float DeltaTime);

	void SetHandbrakeInput(bool bInHandbrakeOn);
	
	UPROPERTY(ReplicatedUsing=OnRep_Steering)
		float ServerSteeringRotation;

	float ClientTimeSinceUpdate;
	float ClientTimeBetweenLastUpdates;
	float ClientStartSteeringRotation;

	UFUNCTION()
		void OnRep_Steering();

private:

	void InstantiateWheel();

	// Stiffness of the suspension in Newtons per meter N/m. A higher stiffness will result in a higher resting length and springier behaviour.
	// This can be thought of as the resistance to a deforming force, such as weight.
	// It should be noted that the heavier the mass, the higher the stiffness needed to support that weight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Core", meta = (AllowPrivateAccess = "true"))
		float Stiffness = 22500.f;

	// This is a dimensionless coefficient and affects the rate at which the suspension spring reaches static equilibrium.
	// A higher damping value will result in less bouncy behaviour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Core", meta = (AllowPrivateAccess = "true"))
		float Damping = 1250.f;

	// This is the maximum length that the spring will extend to from it's starting location. Units are in centimeters (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Core", meta = (AllowPrivateAccess = "true"))
		float ExtensionLength = 50.f;

	//Allows contact forces to be applied, even if no contact is present. This should almost always be left off and is an unsafe option. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Core", meta = (AllowPrivateAccess = "true"))
		bool bOverrideContact = false;

	// Whether or not this wheel will have steering input applied to.
	UPROPERTY(EditAnywhere, Category = "Suspension|Core", meta = (AllowPrivateAccess = "true"))
		bool bIsSteeringWheel = false;

	UPROPERTY(EditAnywhere, Category = "Suspension|Core", meta = (AllowPrivateAccess = "true"))
		TEnumAsByte<ECollisionChannel>  CollisionChannel = ECollisionChannel::ECC_WorldStatic;

	//Allows the suspension component to be rotated about any axis. Do not enable if rotation glitches are observed, and rotate the wheel mesh component instead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Core", meta = (AllowPrivateAccess = "true"))
		bool bAllowFullRotation = false;

	// Prevents the suspension component from applying any forces. This is only useful in certain specific cases and should generally be off. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Core", meta = (AllowPrivateAccess = "true"))
		bool bZeroForceComponent = false;

	//This allows a wheel mesh components to be used in both editor and runtime.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Wheel", meta = (AllowPrivateAccess = "true"))
		bool bInheritMeshFromComponent = false;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Wheel", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bInheritMeshFromComponent==true", EditConditionHides))
	//	FName WheelTag = FName("None");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Wheel", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bInheritMeshFromComponent==false", EditConditionHides))
		UStaticMesh* WheelMesh;

	//Sets the rendering of the wheel static mesh to invisible. This is only called once, in Begin Play.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Wheel", meta = (AllowPrivateAccess = "true"))
	bool bHideWheelMesh = false;

	// This is a purely visual variable. If the wheel is facing in the wrong direction (ussually the left set of wheels), set this to true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Wheel", meta = (AllowPrivateAccess = "true"))
		bool bRotateWheel = false;

	// This can be used to offset the wheel location in the Z Axis. This has no effect on physics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Suspension|Wheel", meta = (AllowPrivateAccess = "true"))
		float WheelZOffset = 0.f;

	//Whether the wheel will be affected by the handbrake
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Wheel|Handbrake", meta = (AllowPrivateAccess = "true"))
		bool bHandbrakeWheel = false;

	//Which axle will the handbrake affect. Since vehicles can have any number of wheels, this must be specified. The axel specified will affect the rotation (physics) behaviour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Handbrake", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bHandbrakeWheel==true", EditConditionHides))
		EHandbrakeWheelType HandbrakeWheelType = EHandbrakeWheelType::Front;

	//No Downscaling 
	float HandbrakeVelocityMultiplier = 1.f;
	bool bHandbrakeInput = false;


	/* Whether to use this with the drivetrain system. Please note that this needs to be enabled in the R-Tune vehicle drivetrain settings as well.
	* Some properties will not be available for modifying during runtime when using this feature. This is due to the nature of drivetrain physics.
	* These properties are:
	* LateralForceScalar
	* WheelAngularVelocityMultiplier
	* BurnoutRotation
	* IdleLock
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"))
		bool bUseWithDrivetrain = false;

	//Whether torque should be applied to this wheel and transmitted
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==true", EditConditionHides))
		bool bDrivenWheel = false;
		
	//This is the percentage of the total available torque that will be transmitted between 0% and 100%. E.g for RWD this value will be 0.5.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (ClampMin = "0"), meta = (ClampMax = "1"), meta = (EditCondition = "bUseWithDrivetrain==true && bDrivenWheel==true", EditConditionHides))
		float TorqueReceived = 0.5f;

	//This is used to calculate the traction (grip). Higher values will reduce slipping.  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==true && bDrivenWheel==true", EditConditionHides))
		float FrictionCoefficient = 1.5f;

	//This is the maximum lateral force when the wheel is not slipping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==true && bDrivenWheel==true", EditConditionHides))
		float MaxLateralForceScalar = 3000.f;

	//This is the minimum lateral tyre force that will be applied when the wheel is slipping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==true && bDrivenWheel==true", EditConditionHides))
		float SlippingLateralForceScalar = 250.f;

	//This is how much more quickly the wheels will maximally spin when slipping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==true && bDrivenWheel==true", EditConditionHides))
		float SlippingAngularVelocity = 4.f;

	//This is the wheel's maximum rotation when the throttle is applied and the wheel is in the air.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==true && bDrivenWheel==true", EditConditionHides))
		float AirRotationMultiplier = 20.f;

	//This is how quickly the wheel will rotate when the throttle is applied and the wheel is in the air.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==true && bDrivenWheel==true", EditConditionHides))
		float InAirSpeedUp = 10.f;

	//This is how quickly the wheel will slow down when the throttle is not being applied and the wheel is in the air.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Drivetrain", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==true && bDrivenWheel==true", EditConditionHides))
		float InAirSlowdownSpeed = 2.f;

	float AppliedTorque = 0.f;
	float FrRatio = 0.f;
	float ThrottleSlipGamma = 0.f;
	bool Gamma = false;

	float DynamicAngularVelocity = 1.f;
	float DynamicPowerReduction = 1.f;
	float DynamicLateralForce = 2000.f;


	// This is a dimensionless scalar. It affects how much lateral force is produced, a higher value will result in more grip.
	// It is imperitive that these values are balanced between all tyres (in SuspensionComponent) of the vehicle. 
	// Higher LateralForceScalar values at the front tyres than the rear will result in oversteer behaviour.
	// Higher LateralForceScalar values at the rear tyres than the front will result in understeer behaviour.
	// It is generally a good idea to set the rear tyres LateralForceScalar higher than at the front for stable behaviour, 
	// but of course depends on your subjective design requirements - in which case anything goes!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Tyre", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==false", EditConditionHides))
		float LateralForceScalar = 2000.f;

	// This is the maximum force that a tyre will produce in Newtons (N).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Tyre", meta = (AllowPrivateAccess = "true"))
		float MaxLateralForce = 8500.f;

	//This is behaves exactly the same as the LateralForceScalar, however it used only for very low speeds for stability.
	// To prevent low speed slipping this value should generally be larger than the LateralForceScalar.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Tyre", meta = (AllowPrivateAccess = "true"))
		float LowSpeedLateralForceScalar = 4000.f;

	//This adds rotation to the wheel as a constant. This is useful for burnout at zero speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Suspension|Burnout", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==false", EditConditionHides))
		float BurnoutRotation = 0.f;

	//For user read only, incase they need it
	float OutCompression = 0.f;

	//UPROPERTY(Replicated)
		float SteeringInput = 0.f;

	//UPROPERTY(Replicated)
		float SensitvityInput = 0.f;

	float SlipAngleDynamic = 0.f;
	bool bLowSpeedOverride = false;



	// This is the distance offset of the force application point to the center of mass. Changing this value affects how much the vehicle will rolls. 
	// Negative values are also valid and may even be necessary to correct for the desired roll amount.
	// It can be seen as somewhat of an anti-roll bar scaling factor.
	UPROPERTY(EditAnywhere, Category = "Suspension|Tyre", meta = (AllowPrivateAccess = "true"))
		float ContactForceOffsetLocation = 0.f;

	// This is simply a scalar for wheel velocity. E.g during a burnout or drifting this can be increased in blueprints to reflect that state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Suspension|Tyre", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==false", EditConditionHides))
		float WheelAngularVelocityMultiplier = 1.f;

	//This can be used to oppose motion in the longitudinal direction applied at the tyre contact point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Tyre", meta = (AllowPrivateAccess = "true"))
		float ResistanceForceCoefficient = 0.f;

	//This is the maximum resistance force in Newtons [N] that can be applied. If the Resistance Force Coefficient is zero, this property will take no effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Tyre", meta = (AllowPrivateAccess = "true"))
		float MaxResistanceForce = 5000.f;


	// This is the maximum angle of wheels set to steer. This affets the lateral force direction, and consequently the vehicle behaviour.
	// Lateral force is applied perpendicular to the forward direction of the wheel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|InputResponse", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bIsSteeringWheel==true", EditConditionHides))
		float MaxSteeringAngle = 40.f;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|InputResponse", meta = (AllowPrivateAccess = "true"))
		float SteeringSpeed = 2.f;

	// This is how quickly the steer wheel rotates to zero degrees. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|InputResponse", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bIsSteeringWheel==true", EditConditionHides))
		float SteeringReleaseSpeed = 12.f;


	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|InputResponse", meta = (AllowPrivateAccess = "true"))
		//float AdditionalWheelVelocity = 0.f;

	// This uses spheretrace and switches to a line raycast when the spheretrace becomes unsuitable. 
	// If this is set to false, a pure line raycast will be used which may be more suitable for arcade behaviour 
	UPROPERTY(EditAnywhere, Category = "Suspension|Raycast", meta = (AllowPrivateAccess = "true"))
		bool bHybridRaycast = true;

	//The radius used for the sphere trace. The radius will automatically be set by the wheelMesh. If no mesh is specified then this value will be used. This value is in meters (m).
	UPROPERTY(EditAnywhere, Category = "Suspension|Raycast", meta = (AllowPrivateAccess = "true"))
		float Radius =33.f;

	// This is one of the conditions for switching to a line raycast. This is the normalized impact normal threshold, values closer to 1 are of a higher tolerance which will result in the trace being predominantly a sphere trace.
	UPROPERTY(EditAnywhere, Category = "Suspension|Raycast", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bHybridRaycast==true", EditConditionHides), meta = (ClampMin = "0"), meta = (ClampMax = "1"))
		float ImpactNormalTolerance = 0.9f;

	// In centimeters (cm). This is the second of the conditions for switching to a line raycast. This is the maximum height of the wheel that is valid for a sphere trace collision.
	// Lower this value to limit the sphere contact region closer to the base of the wheel.
	UPROPERTY(EditAnywhere, Category = "Suspension|Raycast", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bHybridRaycast==true", EditConditionHides))
		float ImpactHeightTolerance = 30.f;
		
	//Whether force is applied back onto physics bodies. If multiple objects are being detected, due to level design choices, custom collision channels need to be used.
	UPROPERTY(EditAnywhere, Category = "Suspension|Raycast", meta = (AllowPrivateAccess = "true"))
		bool bApplyContactForceOnObjects = false;


	UPROPERTY(EditAnywhere, Category = "Suspension|Raycast", meta = (AllowPrivateAccess = "true"))
	bool bDebug = false;

	int Direction = 0;
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|Wheel", meta = (AllowPrivateAccess = "true"))
		//ESteeringType SteeringType = ESteeringType::EST_None;

	//PROPERTY(EditAnywhere, Category = "Wheel", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WheelMeshComponent;

	float WheelRadius = 0.315f;

	float SteeringRotation = 0.f;
	float WheelRelativeOffset = 0.f;
	FRotator WheelLocalRotation = FRotator::ZeroRotator;

#pragma region TimeSteps

	float AsyncTime = 0.f;
	float GameTime = 0.f;

#pragma endregion

	UPrimitiveComponent* VehiclePrimitiveComponent;
	FTransform PhysicsTransform;
	//FBodyInstance* PhysicsBody;
	//FBodyInstance* VehiclePhysicsBody;
	FCollisionShape SweepShape;
	FVector ImpactNormal = FVector::ZeroVector;
	float ImpactDot = 0.f;

	FHitResult Hit;
	FHitResult LinearHit;
	float LastLength = 0.f;
	float RestLength = 0.f;
	float CurrentLength = 40.f;
	float LastValidLength = 0.f;
	float PreviousCompression = 0.f;
	FVector LinearVelocity = FVector::ZeroVector;
	FVector VelocityVector = FVector::ZeroVector;
	bool bSingleRaycast = false;

	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;

	float wheelAnimZ = 0.f;
	FRotator angularAnimRotator = FRotator::ZeroRotator;

#pragma region IdleLock

	// Whether or not to lock to wheels animation when completely stationary to prevent micro rotations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|IdleLock", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==false", EditConditionHides))
		bool bUseIdleLock = true;

	// How long after stationary to lock the wheels. In seconds (s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension|IdleLock", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bUseWithDrivetrain==false", EditConditionHides))
		float IdleLockTime = 1.5f;

	bool bIsIdleLocked = false;

	float IdleLockCount = 0;

	float IdleLockSpeedThreshold = 1.f;

#pragma endregion

	friend class AVehicleDebugHUD;
};
