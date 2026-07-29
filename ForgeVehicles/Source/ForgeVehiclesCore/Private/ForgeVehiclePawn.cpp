// Copyright 2017-2020 Puny Human, All Rights Reserved.

#include "ForgeVehiclesCore.h"
#include "ForgeVehiclePawn.h"
#include "ForgeVehicleSeatConfig.h"
#include "GameFramework/PlayerState.h"
#include "EngineMinimal.h"

// Sets default values
AForgeVehiclePawn::AForgeVehiclePawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AForgeVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AForgeVehiclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AForgeVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AForgeVehiclePawn::BecomePossessedByPlayer(APlayerState* InPlayerState)
{
	if (!IsValid(InPlayerState))
	{
		return;
	}
	if (AController* OtherController = Cast<AController>(InPlayerState->GetOwner()))
	{
		APawn* PreviousPawn = OtherController->GetPawn();

		OtherController->Possess(this);

		PreviousPawn->SetOwner(OtherController);
	}
}

AForgeBaseVehicle* AForgeVehiclePawn::GetOwningVehicle()
{
	return GetSeatConfig()->GetVehicleOwner();
}

void AForgeVehiclePawn::NotifyPlayerSeatChangeEvent_Implementation(APlayerState* Player, UForgeVehicleSeatConfig* ToSeat, UForgeVehicleSeatConfig* FromSeat, EForgeVehicleSeatChangeType SeatChangeEvent)
{

}

