// Copyright 2017-2020 Puny Human, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"
#include "ForgeVehicleDeveloperSettings.generated.h"

/**
 * 
 */
UCLASS(Config=Game, defaultconfig)
class FORGEVEHICLESCORE_API UForgeVehicleDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UForgeVehicleDeveloperSettings(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle Config", config)
	TSubclassOf<class UForgeVehiclePlayerSeatComponent> PlayerSeatComponentClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle Config", config)
	TSubclassOf<class UForgeVehiclePlayerStateComponent> PlayerStateComponentClass;
};
