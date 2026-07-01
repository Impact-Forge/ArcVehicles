//Copyright 2025 P.Kallisto 

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "ArticulatedComponent.generated.h"



UENUM(BlueprintType)
enum class EArticulationAxis : uint8
{
	X UMETA(Display = "X Location"),
	Y UMETA(Display = "Y Location"),
	Z UMETA(Display = "Z Location"),
	Roll UMETA(Display = "Roll Rotation"),
	Pitch UMETA(Display = "Pitch Rotation"),
	Yaw UMETA(Display = "Yaw Rotation")
};
/**
 * 
 */
UCLASS(ClassGroup = (ForgeVehiclesGround), meta = (BlueprintSpawnableComponent, DisplayName = "Articulated Component"))
class FORGEVEHICLESGROUND_API UArticulatedComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	UArticulatedComponent();

	virtual void BeginPlay() override;

	void Update(float deltaTime);

	UFUNCTION(BlueprintCallable, Category = "ForgeVehiclesGround")
	void SetInput(float value);

	UFUNCTION(BlueprintPure, Category = "ForgeVehiclesGround")
	float GetMovement();

protected:

	//This is the movement axis. Only one axis/rotation can be animated at at time
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Articulation", meta = (AllowPrivateAccess = "true"))
	EArticulationAxis Axis = EArticulationAxis::Z;

	//Whether or not to clamp the movement. Setting this to false is useful for rotational components.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Articulation", meta = (AllowPrivateAccess = "true"))
	bool bClamped = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Articulation", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bClamped==true", EditConditionHides))
	float Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Articulation", meta = (AllowPrivateAccess = "true"), meta = (EditCondition = "bClamped==true", EditConditionHides))
	float Max= 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Articulation", meta = (AllowPrivateAccess = "true"))
	float MovementSpeed = 1.f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Articulation", meta = (AllowPrivateAccess = "true"))
	bool bUseSmoothing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Articulation", meta = (AllowPrivateAccess = "true"))
	float SmoothingSpeed = 10.f;

	float InputValue = 0.f;
	float Movement = 0.f;
	float SmoothMovement = 0.f;
	
	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;
};
