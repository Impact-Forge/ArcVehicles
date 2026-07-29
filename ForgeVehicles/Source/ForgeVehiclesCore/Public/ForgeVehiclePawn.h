// Copyright 2017-2020 Puny Human, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ForgeVehicleTypes.h"
#include "ForgeVehiclePawn.generated.h"

class UForgeVehicleSeatConfig;

//Base Pawn class for all vehicle objects.  This diverges between Vehicles and the Seat Pawns
UCLASS(Abstract)
class FORGEVEHICLESCORE_API AForgeVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AForgeVehiclePawn();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Forge|Vehicle")

	virtual UForgeVehicleSeatConfig* GetSeatConfig() PURE_VIRTUAL(AForgeVehiclePawn::GetSeatConfig(), return nullptr;);

	virtual void BecomePossessedByPlayer(APlayerState* InPlayerState);

	UFUNCTION(BlueprintPure, Category="Forge|Vehicle")
	virtual AForgeBaseVehicle* GetOwningVehicle();


	UFUNCTION(BlueprintNativeEvent)
	void NotifyPlayerSeatChangeEvent(APlayerState* Player, UForgeVehicleSeatConfig* ToSeat, UForgeVehicleSeatConfig* FromSeat, EForgeVehicleSeatChangeType SeatChangeEvent);
	virtual void NotifyPlayerSeatChangeEvent_Implementation(APlayerState* Player, UForgeVehicleSeatConfig* ToSeat, UForgeVehicleSeatConfig* FromSeat, EForgeVehicleSeatChangeType SeatChangeEvent);
};
