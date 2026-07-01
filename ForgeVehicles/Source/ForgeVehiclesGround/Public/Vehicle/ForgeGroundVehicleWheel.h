//Copyright 2024 P.Kallisto 

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "ForgeGroundVehicleWheel.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (ForgeVehiclesGround), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESGROUND_API UForgeGroundVehicleWheel : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	UForgeGroundVehicleWheel();

	bool IsLeftWheel();

private:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeVehiclesGround|Animation System", meta = (AllowPrivateAccess = "true"))
		bool bRotateWheel = false;

};
