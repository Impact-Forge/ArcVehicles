// Copyright 2017-2020 Puny Human, All Rights Reserved.


#include "ForgeVehicleBPFunctionLibrary.h"

AForgeBaseVehicle* UForgeVehicleBPFunctionLibrary::GetVehicleFromSeatConfig(FForgeVehicleSeatReference SeatRef)
{
	return SeatRef.Vehicle;
}

UForgeVehicleSeatConfig* UForgeVehicleBPFunctionLibrary::GetVehicleSeatConfigFromRef(FForgeVehicleSeatReference SeatRef)
{
	return *SeatRef;
}

bool UForgeVehicleBPFunctionLibrary::IsSeatRefValid(FForgeVehicleSeatReference SeatRef)
{
	return SeatRef.IsValid();
}
