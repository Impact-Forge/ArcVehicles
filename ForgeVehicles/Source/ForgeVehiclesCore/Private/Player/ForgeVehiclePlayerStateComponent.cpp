// Copyright 2017-2020 Puny Human, All Rights Reserved.


#include "Player/ForgeVehiclePlayerStateComponent.h"

// Sets default values for this component's properties
UForgeVehiclePlayerStateComponent::UForgeVehiclePlayerStateComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicated(false);
}

