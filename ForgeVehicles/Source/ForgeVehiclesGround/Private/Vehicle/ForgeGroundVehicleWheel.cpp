//Copyright 2024 P.Kallisto 


#include "Vehicle/ForgeGroundVehicleWheel.h"

UForgeGroundVehicleWheel::UForgeGroundVehicleWheel()
{
	ComponentTags.Add(FName("ForgeVehiclesGroundWheel"));
}

bool UForgeGroundVehicleWheel::IsLeftWheel()
{
	return bRotateWheel;
}
