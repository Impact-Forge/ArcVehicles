// Copyright 2017-2020 Puny Human, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForgeVehicleTypes.h"
#include "ForgeVehiclePlayerSeatComponent.generated.h"

class UForgeVehicleSeatConfig;
class APlayerState;



UCLASS(ClassGroup = (ForgeVehiclesCore), meta = (BlueprintSpawnableComponent), Blueprintable)
class FORGEVEHICLESCORE_API UForgeVehiclePlayerSeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UForgeVehiclePlayerSeatComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	virtual void OnRegister() override;

	virtual void ChangeSeats(const FForgeVehicleSeatReference& NewSeat);

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Vehicle", ReplicatedUsing = OnRep_SeatConfig)
	FForgeVehicleSeatReference CurrentSeatConfig;
	//	UForgeVehicleSeatConfig* SeatConfig;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	FForgeVehicleSeatReference PreviousSeatConfig;
	//	UForgeVehicleSeatConfig* PreviousSeatConfig;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Vehicle", ReplicatedUsing = OnRep_StoredPlayerState)
		APlayerState* StoredPlayerState;

	FForgeVehicleScopedRelativeTransformRestoration RelativeTransformRestorer;

	UFUNCTION()
		virtual void OnRep_SeatConfig(const FForgeVehicleSeatReference& InPreviousSeatConfig);

	UFUNCTION()
	virtual void OnRep_StoredPlayerState(APlayerState* InPreviousPlayerState);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnRep_StoredPlayerState(APlayerState* InPreviousPlayerState);

	UFUNCTION(BlueprintNativeEvent)
		void OnSeatChangeEvent(EForgeVehicleSeatChangeType SeatChangeType);
	void OnSeatChangeEvent_Implementation(EForgeVehicleSeatChangeType SeatChangeType);

	virtual void SetIgnoreBetween(AActor* OtherActor);

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

	UPROPERTY(ReplicatedUsing=OnRep_ServerDebugStrings)
	TArray<FString> ServerDebugStrings;

	UFUNCTION()
	void OnRep_ServerDebugStrings();

	EForgeVehicleSeatChangeType DebugLastSeatChangeType;

protected:
	UPROPERTY()
		TMap<UPrimitiveComponent*, TEnumAsByte<ECollisionResponse>> PreviousVehicleCollisionResponses;
};
