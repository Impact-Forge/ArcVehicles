// Copyright 2017-2020 Puny Human, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ForgeVehiclePawn.h"
#include "ForgeVehicleSeat.generated.h"



UCLASS(Abstract)
class FORGEVEHICLESCORE_API AForgeVehicleSeat : public AForgeVehiclePawn
{
	GENERATED_BODY()

public:
	friend class AForgeBaseVehicle;
	friend class UForgeVehicleSeatConfig;
	friend class UForgeVehicleSeatConfig_SeatPawn;

	// Sets default values for this pawn's properties
	AForgeVehicleSeat();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual UForgeVehicleSeatConfig* GetSeatConfig() override;

protected:

	UPROPERTY()
	UForgeVehicleSeatConfig* SeatConfig;

};
