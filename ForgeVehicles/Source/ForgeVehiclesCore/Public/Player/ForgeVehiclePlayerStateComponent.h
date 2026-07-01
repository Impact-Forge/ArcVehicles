// Copyright 2017-2020 Puny Human, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForgeVehiclePlayerStateComponent.generated.h"


UCLASS( ClassGroup=(ForgeVehiclesCore), meta=(BlueprintSpawnableComponent), Blueprintable )
class FORGEVEHICLESCORE_API UForgeVehiclePlayerStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UForgeVehiclePlayerStateComponent();

	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category="Vehicle")
	APawn* StoredPlayerPawn;

};
