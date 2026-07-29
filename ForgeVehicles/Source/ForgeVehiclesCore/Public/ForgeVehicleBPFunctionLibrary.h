// Copyright 2017-2020 Puny Human, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ForgeVehicleTypes.h"
#include "ForgeVehicleBPFunctionLibrary.generated.h"

class AForgeBaseVehicle;

/**
 * 
 */
UCLASS()
class FORGEVEHICLESCORE_API UForgeVehicleBPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintPure, Category="ForgeVehiclesCore")
	static AForgeBaseVehicle* GetVehicleFromSeatConfig(FForgeVehicleSeatReference SeatRef);

	UFUNCTION(BlueprintPure, Category = "ForgeVehiclesCore")
	static UForgeVehicleSeatConfig* GetVehicleSeatConfigFromRef(FForgeVehicleSeatReference SeatRef);

	UFUNCTION(BlueprintPure, Category = "ForgeVehiclesCore")
	static bool IsSeatRefValid(FForgeVehicleSeatReference SeatRef);
};
