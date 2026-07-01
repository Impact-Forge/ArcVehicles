//Copyright 2024 H.Kallisto

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"

#include "RotorComponent.generated.h"


UENUM(BlueprintType)
enum class ERotorAxis : uint8
{
	X UMETA(DisplayName = "X Roll"),
	Y UMETA(DisplayName = "Y Pitch"),
	Z UMETA(DisplayName = "Z Yaw")
};


UENUM(BlueprintType)
enum class EImplementation : uint8
{
	EAnimationOnly UMETA(DisplayName = "Animation Only"),
	EPhysicsOnly UMETA(DisplayName = "Physics Only"),
	EPhysicsWithAnimation UMETA(DisplayName = "Physics and Animation")
};

UENUM(BlueprintType)
enum class EThrustImplementation : uint8
{
	ETI_Constant UMETA(DisplayName = "Constant"),
	ETI_Dynamic UMETA(DisplayName = "Dynamic")
};

UCLASS()
class FORGEVEHICLESROTARYWING_API URotorComponent : public UStaticMeshComponent
{
	GENERATED_BODY()
	
public:

	void UpdatePhysics(float deltaTime, UStaticMeshComponent* physicsBody, float takeoffRatio);

	void UpdateAnimation(float currentSpeed, float takeoffRatio);


protected:

	virtual void BeginPlay() override;

private:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	float DynamicThrust = 0.f;
	//How the rotor should behave during runtime.
	//Animation Only - No thrust will be produced. The rotor will be purely cosmetic.
	//Physics Only - Thrust will be produced at the location of the rotor relative to the core mesh, and in the forward direction of the mesh. The rotor mesh will not be animated. 
	//Physics and Animation - Thrust will be produced as well as animation. This is a combination of the previous options.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Main", meta = (AllowPrivateAccess = "true"))
		EImplementation Implementation = EImplementation::EAnimationOnly;

	//Whether or not the rotor animation, and force, is affected by take-off and landing. if this is disabled the rotors will rotate regardless of landing/take-off, and thrust will be produced.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Main", meta = (AllowPrivateAccess = "true"))
		bool bAffectedByTakeoffAndLanding = true;

	//Axis of rotation of the rotor mesh. If the rotor seems to be rotating about the incorrect axis, this should be changed to faciliate the principle rotation axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Animation", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "Implementation==EImplementation::EAnimationOnly||Implementation==EImplementation::EPhysicsWithAnimation", EditConditionHides))
		ERotorAxis RotationAxis = ERotorAxis::Z;

	//Inverts the rotation of the rotor. E.g Clockwise to anti-clockwise.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Animation", meta = (AllowPrivateAccess = "true"),meta = (EditCondition = "Implementation==EImplementation::EAnimationOnly||Implementation==EImplementation::EPhysicsWithAnimation", EditConditionHides))
		bool bInvertDirection = false;

	//The minimum rotation speed of the rotor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Animation", meta = (AllowPrivateAccess = "true"),meta = (EditCondition = "Implementation==EImplementation::EAnimationOnly||Implementation==EImplementation::EPhysicsWithAnimation", EditConditionHides))
		float MinRotorSpeed = 20.f;

	//The maximum rotation speed of the rotor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Animation", meta = (AllowPrivateAccess = "true"),meta = (EditCondition = "Implementation==EImplementation::EAnimationOnly||Implementation==EImplementation::EPhysicsWithAnimation", EditConditionHides))
		float MaxRotorSpeed = 50.f;

	//This can be used to increase, or decrease, the rotation speed of the rotor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Animation", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "Implementation==EImplementation::EAnimationOnly||Implementation==EImplementation::EPhysicsWithAnimation", EditConditionHides))
		float RotorSpeedMultiplier = 1.f;

	//This is how thrust will be applied by the rotor. 
	//Constant - Force will be applied constantly with no external factors influencing it. It can be changed during runtime.
	//Dynamic - Force is applied based on an input. If this is selection, use 'SetThrustInput' to modify the thrust.
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Physics", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "Implementation==EImplementation::EPhysicsOnly||Implementation==EImplementation::EPhysicsWithAnimation", EditConditionHides))
		//EThrustImplementation ThrustImplementation = EThrustImplementation::ETI_Constant;

	//This is the amount of thrust produced in Newtons. This can be used as a static thrust or modified during runtime for more dynamic behaviour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Physics", meta = (AllowPrivateAccess = "true"),meta = (EditCondition = "Implementation==EImplementation::EPhysicsOnly||Implementation==EImplementation::EPhysicsWithAnimation", EditConditionHides))
		float Thrust = 5000.f;

	//Invert the direction of force. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Physics", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "Implementation==EImplementation::EPhysicsOnly||Implementation==EImplementation::EPhysicsWithAnimation", EditConditionHides))
		bool bInvertForceDirection = false;

	//Show debug force vectors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Debug Physics", meta = (AllowPrivateAccess = "true"))
		bool bShowDebugData = false;

	//This is the downscaling of the debug line to fit on screen. This value should be roughly half the size of the force used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Debug Physics", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bShowDebugData==true", EditConditionHides))
		float DebugScale = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor|Debug Physics", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bShowDebugData==true", EditConditionHides))
		float LineThickness = 15.f;
};
