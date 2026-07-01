// Copyright Impact-Forge.

#pragma once

#include "CoreMinimal.h"
#include "ForgeVehicleSeatConfig.h"
#include "ForgeVehicleTypes.h"
#include "ForgeSeatConfig.generated.h"

/**
 * Seat-pawn configuration that additionally carries FForgeSeatData describing how the occupant should
 * behave while seated (input context, held-item visibility, seated animation, camera and view clamps).
 * AForgeVehicle reads this via GetSeatData() when a player enters, switches or leaves the seat.
 */
UCLASS(EditInlineNew, Blueprintable, BlueprintType, DisplayName = "Forge Seat Config")
class FORGEVEHICLESCORE_API UForgeSeatConfig : public UForgeVehicleSeatConfig_SeatPawn
{
	GENERATED_BODY()

public:

	/* Presentation/behaviour data applied to whoever occupies this seat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Seat")
	FForgeSeatData SeatData;
};
