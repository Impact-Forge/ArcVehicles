// Copyright 2017-2020 Puny Human, All Rights Reserved.


#include "ForgeVehicleDeveloperSettings.h"
#include "Player/ForgeVehiclePlayerSeatComponent.h"
#include "Player/ForgeVehiclePlayerStateComponent.h"

UForgeVehicleDeveloperSettings::UForgeVehicleDeveloperSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerSeatComponentClass = UForgeVehiclePlayerSeatComponent::StaticClass();
	PlayerStateComponentClass = UForgeVehiclePlayerStateComponent::StaticClass();
}
