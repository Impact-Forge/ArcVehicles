// Copyright 2017-2020 Puny Human, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ForgeVehiclePawn.h"
#include "ForgeBaseVehicle.generated.h"

class UForgeVehicleSeatConfig;
class APlayerState;

USTRUCT()
struct FORGEVEHICLESCORE_API FForgeVehicleSeatChangeEvent
{
	GENERATED_USTRUCT_BODY()
public:
	static int32 ANY_SEAT;
	static int32 NO_SEAT;

	int32 FromSeat;
	int32 ToSeat;

	bool bFindEmptySeatOnFail;

	bool bIgnoreAnyRestrictions;

	UPROPERTY()
	APlayerState* Player = nullptr;

};

UCLASS()
class FORGEVEHICLESCORE_API AForgeBaseVehicle : public AForgeVehiclePawn
{
	GENERATED_BODY()

public:
	friend struct FForgeVehicleSeatReference;

	// Sets default values for this pawn's properties
	AForgeBaseVehicle();

	virtual void PostInitProperties() override;
	virtual void PostInitializeComponents() override;

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags);

	virtual void PostNetReceivePhysicState() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void Restart();

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


	virtual UForgeVehicleSeatConfig* GetSeatConfig() override;

	UFUNCTION(BlueprintPure, Category = "ForgeVehiclesCore|Vehicle")
	virtual UForgeVehicleSeatConfig* GetDriverSeat();

	virtual AForgeBaseVehicle* GetOwningVehicle() override;
	

	virtual void SetupVehicleSeats();

	UFUNCTION(BlueprintNativeEvent)
	void SetupSeat(UForgeVehicleSeatConfig* SeatConfig);
	virtual void SetupSeat_Implementation(UForgeVehicleSeatConfig* SeatConfig);

	
	UFUNCTION(BlueprintCallable, Category = "ForgeVehiclesCore|Vehicle")
	virtual void GetAllSeats(TArray<UForgeVehicleSeatConfig*>& Seats);

	virtual bool CanProcessSeatChange(const FForgeVehicleSeatChangeEvent& SeatChange);
	
	//Returns a valid seat if found a player in that seat, or nullptr if no seat is found.
	//If you pass nullptr for Player, then it will find the first open seat, or nullptr if all seats are full
	virtual UForgeVehicleSeatConfig* FindSeatContainingPlayer(APlayerState* Player);

	UFUNCTION(BlueprintCallable, Category = "ForgeVehiclesCore|Vehicle")
	virtual void RequestEnterAnySeat(APlayerState* InPlayerState);

	UFUNCTION(BlueprintCallable, Category = "ForgeVehiclesCore|Vehicle")
	virtual void RequestLeaveVehicle(APlayerState* InPlayerState);

	UFUNCTION(BlueprintCallable, Category = "ForgeVehiclesCore|Vehicle")
	virtual void RequestEnterSeat(APlayerState* InPlayerState, int32 RequestedSeatIndex, bool bIgnoreRestrictions = false);

	UFUNCTION(BlueprintPure, Category = "ForgeVehiclesCore|Vehicle")
	bool IsValidSeatIndex(int32 InSeatIndex) const;

	UForgeVehicleSeatConfig* GetSeatConfig(const FForgeVehicleSeatReference& SeatRef);
	FForgeVehicleSeatReference GetSeatReference(UForgeVehicleSeatConfig* SeatConfig);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ForgeVehiclesCore|Vehicle")
	FTransform GetNearestExitTransform(FTransform InputLocation);
	virtual FTransform GetNearestExitTransform_Implementation(FTransform InputLocation);

	UFUNCTION(BlueprintPure, Category = "ForgeVehiclesCore|Vehicle")
	virtual void GetSortedExitPoints(FTransform InputLocation, TArray<FTransform>& OutTransformArray) const;

	virtual void PushSeatChangeEvent(const FForgeVehicleSeatChangeEvent& SeatChangeEvent);
	virtual void GetAllVehicleActors(TArray<AActor*>& VehicleActors);

	virtual void NotifyPlayerSeatChangeEvent_Implementation(APlayerState* Player, UForgeVehicleSeatConfig* ToSeat, UForgeVehicleSeatConfig* FromSeat, EForgeVehicleSeatChangeType SeatChangeEvent);

public:

	//Seat Configuration for the driver.  This object is always valid and must exist for the vehicle to be driveable
	//
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle Config", Instanced)
	UForgeVehicleSeatConfig* DriverSeatConfig;

	//Additional Seat Configurations for this vehicle
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle Config", Instanced)
	TArray<UForgeVehicleSeatConfig*> AdditionalSeatConfigs;

	UPROPERTY(Replicated, Transient)
	TArray<UForgeVehicleSeatConfig*> ReplicatedSeatConfigs;

private:
	void ProcessSeatChangeQueue();
		
	UPROPERTY()
	TArray<FForgeVehicleSeatChangeEvent> SeatChangeQueue;

	int32 GetSeatIndex(UForgeVehicleSeatConfig* Seat);

protected:
	virtual void UpdatePhysicsIgnores();


public:
	static void OnShowDebugInfo(class AHUD* HUD, class UCanvas* Canvas, const class FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);


	/**
	* Draw important variables on canvas.
	*
	* @param Canvas - Canvas to draw on
	* @param DebugDisplay - Contains information about what debug data to display
	* @param YL - Height of the current font
	* @param YPos - Y position on Canvas. YPos += YL, gives position to draw text for next debug line.
	*/
	virtual void DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

	virtual void GenerateDebugStrings(TArray<FString>& OutStrings);

	/** Ask the server to send ability system debug information back to the client, via ClientPrintDebug_Response  */
	UFUNCTION(Server, reliable, WithValidation)
		void ServerPrintDebug_Request();
	void ServerPrintDebug_Request_Implementation();
	bool ServerPrintDebug_Request_Validate();

	virtual bool ShouldRequestDebugStrings() const;

	UPROPERTY(ReplicatedUsing = OnRep_ServerDebugStrings)
		TArray<FString> ServerDebugStrings;

	UFUNCTION()
		void OnRep_ServerDebugStrings();

};
