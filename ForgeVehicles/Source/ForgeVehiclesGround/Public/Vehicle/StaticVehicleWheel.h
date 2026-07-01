//Copyright 2024 P.Kallisto 

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "StaticVehicleWheel.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (ForgeVehiclesGround), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESGROUND_API UStaticVehicleWheel : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	UStaticVehicleWheel();

	UFUNCTION(BlueprintCallable, Category = "Static Vehicle Wheel")
		void Update(float deltaTime);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	
private:


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel", meta = (AllowPrivateAccess = "true"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel", meta = (AllowPrivateAccess = "true"))
	bool bInvertDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel", meta = (AllowPrivateAccess = "true"))
	float RotationMultiplier = 1.f;

	UPrimitiveComponent* VehiclePrimitiveComponent;

	FVector minBound = FVector::ZeroVector;
	FVector maxBound = FVector::ZeroVector;
};
