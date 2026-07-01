//Copyright 2025 P.Kallisto 

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "ForgeGroundInteriorComponent.generated.h"

UENUM(BlueprintType)
enum class EForgeGroundAnimationType : uint8
{
	RPM UMETA(Display = "RPM"),
	Speed UMETA(Display = "Speed"),
	Steering UMETA(Display = "Steering")
};

UENUM(BlueprintType)
enum class ERotatorAxis : uint8
{
	Roll UMETA(Display = "Roll"),
	Pitch UMETA(Display = "Pitch"),
	Yaw UMETA(Display = "Yaw")
};

UCLASS(ClassGroup = (ForgeVehiclesGround), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESGROUND_API UForgeGroundInteriorComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	UForgeGroundInteriorComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Update(float deltaTime, float inRPM, float inSpeed, float inSteering);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	EForgeGroundAnimationType AnimationType = EForgeGroundAnimationType::RPM;

	/*This is the range for the data being used to drive animation(RPM, Speed, Steering).
	* E.g for RPM the minimum (X) will be the idle RPM, and the maximum (Y) will be the max rpm.
	* For Steering the minimum (X) will be -1, and maximum 1 (Y)
	* For Speed the minimum (X) will be 0, and maximum (Y) will be the top speed 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	FVector2D InputRange;

	/*This is the angle range that the data will be mapped to. The minimum angle (X) will be the resting angle.
	The units are in degrees*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	FVector2D OutputRange;

	//The rotation axis of the component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	ERotatorAxis RotationAxis = ERotatorAxis::Roll;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bUseSmoothing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	float SmoothingSpeed = 5.f;

	//Inverts rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bInvert = false;

private:

	FRotator BaseRotation = FRotator::ZeroRotator;
	float RPM = 0.f;
	float Speed = 0.f;
	float Steering = 0.f;
	float Rotation = 0.f;
	
};
