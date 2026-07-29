// Copyright 2017-2020 Puny Human, All Rights Reserved.


#include "Seats/ForgeVehicleSeat.h"

// Sets default values
AForgeVehicleSeat::AForgeVehicleSeat()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AForgeVehicleSeat::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AForgeVehicleSeat::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AForgeVehicleSeat::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

UForgeVehicleSeatConfig* AForgeVehicleSeat::GetSeatConfig()
{
	return SeatConfig;
}

