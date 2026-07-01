// Copyright 2017-2020 Puny Human, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/NoExportTypes.h"
#include "ForgeVehicleTypes.h"
#include "ForgeVehicleSeatConfig.generated.h"

class UForgeVehiclePlayerSeatComponent;
class AForgeVehiclePawn;

/**
 *
 */
UCLASS(EditInlineNew, Abstract, Blueprintable, BlueprintType)
class FORGEVEHICLESCORE_API UForgeVehicleSeatConfig : public UObject
{
	GENERATED_BODY()
public:

	UForgeVehicleSeatConfig();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const override;
	virtual bool IsSupportedForNetworking() const override;

	//Where the seat is attached to on the parent vehicle
	UPROPERTY(EditAnywhere, Category = "Attach", Replicated)
		FForgeOwnerAttachmentReference AttachSeatToComponent;

	UPROPERTY(VisibleInstanceOnly, Category = "Seat")
		APlayerState* PlayerInSeat;

	UPROPERTY()
		UForgeVehiclePlayerSeatComponent* PlayerSeatComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Character", Replicated)
		bool bPlayerVisible;

	UFUNCTION(BlueprintPure)
		virtual bool IsOpenSeat() const;

	UFUNCTION(BlueprintPure)
		class AForgeBaseVehicle* GetVehicleOwner() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
		void SetupSeatAttachment();
	virtual void SetupSeatAttachment_Implementation();

	UFUNCTION()
		virtual void AttachPlayerToSeat(APlayerState* Player);


	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Attach Player To Seat"))
		void BP_AttachPlayerToSeat(APlayerState* Player);

	UFUNCTION()
		virtual void UnAttachPlayerFromSeat(APlayerState* Player);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Unattach Player To Seat"))
		void BP_UnAttachPlayerFromSeat(APlayerState* Player);

	virtual AForgeVehiclePawn* GetSeatPawn() const;

	virtual FTransform GetSeatAttachTransform_World();
	virtual FTransform GetSawnAttachTrasnform_Relative();
	
	UFUNCTION(BlueprintPure)
		bool IsDriverSeat() const;
};

UCLASS()
class FORGEVEHICLESCORE_API UForgeVehicleSeatConfig_PlayerAttachment : public UForgeVehicleSeatConfig
{
	GENERATED_BODY()
public:

	virtual void AttachPlayerToSeat(APlayerState* Player) override;
	//TODO: Animation Stuff



};


UCLASS()
class FORGEVEHICLESCORE_API UForgeVehicleSeatConfig_SeatPawn : public UForgeVehicleSeatConfig_PlayerAttachment
{
	GENERATED_BODY()
public:
	UForgeVehicleSeatConfig_SeatPawn();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Seat Pawn", Replicated)
		TSubclassOf<class AForgeVehicleSeat> SeatPawnClass;

	/** Property to point to the template child actor for details panel purposes */
	//UPROPERTY(VisibleDefaultsOnly, DuplicateTransient, Category = "Seat Pawn", meta = (ShowInnerProperties))
	//AForgeVehicleSeat* SeatPawnTemplate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Character", Replicated)
		FForgeOwnerAttachmentReference PlayerCharacterAttachToComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Character", Replicated)
		bool bResetControlRotationOnEnter;

	UPROPERTY(ReplicatedUsing = OnRep_SeatPawn)
		AForgeVehiclePawn* SeatPawn;

	UFUNCTION()
		void OnRep_SeatPawn(AForgeVehiclePawn* OldSeatPawn);

	virtual void SetupSeatAttachment_Implementation() override;
	virtual void AttachPlayerToSeat(APlayerState* Player) override;

	virtual AForgeVehiclePawn* GetSeatPawn() const override;
};
