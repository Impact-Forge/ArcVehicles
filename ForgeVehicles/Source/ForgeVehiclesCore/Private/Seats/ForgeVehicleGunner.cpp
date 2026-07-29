// Copyright Impact-Forge. Gunner seat pawn, reformatted for Forge from the BurningLands ABLVehicleGunner.

#include "Seats/ForgeVehicleGunner.h"

#include "ForgeBaseVehicle.h"
#include "ForgeVehicleSeatConfig.h"
#include "Player/ForgeVehiclePlayerSeatComponent.h"

#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

AForgeVehicleGunner::AForgeVehicleGunner()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AForgeVehicleGunner::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	// Drive the camera from the possessing controller's view so the gunner can aim freely.
	OutResult.Location = GetPawnViewLocation();
	OutResult.Rotation = GetViewRotation();
	OutResult.FOV = CameraFOV;
}

FVector AForgeVehicleGunner::GetPawnViewLocation() const
{
	if (!GetRootComponent())
	{
		return GetActorLocation();
	}

	const double ActorHeight = GetRootComponent()->CalcBounds(GetRootComponent()->GetComponentTransform()).BoxExtent.Z * 2.0;
	return GetActorLocation() + FVector(0.0, 0.0, ActorHeight);
}

#if WITH_EDITOR
bool AForgeVehicleGunner::IsSelectable() const
{
	// Gunner pawns are spawned as previews on vehicles in the editor; don't let them be selected there.
	return IsValid(GetWorld())
		&& GetWorld()->WorldType != EWorldType::Editor
		&& GetWorld()->WorldType != EWorldType::EditorPreview;
}
#endif // WITH_EDITOR

void AForgeVehicleGunner::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AForgeVehicleGunner::Input_MouseLook);
		}
	}
}

void AForgeVehicleGunner::BecomePossessedByPlayer(APlayerState* InPlayerState)
{
	// Resolve which seat config points at this gunner pawn so GetSeatConfig() is valid once possessed.
	if (!SeatConfig)
	{
		if (AForgeBaseVehicle* Vehicle = Cast<AForgeBaseVehicle>(GetOwner()))
		{
			TArray<UForgeVehicleSeatConfig*> Seats;
			Vehicle->GetAllSeats(Seats);

			for (UForgeVehicleSeatConfig* Seat : Seats)
			{
				if (Seat && Seat->GetSeatPawn() == this)
				{
					SeatConfig = Seat;
					break;
				}
			}
		}
	}

	Super::BecomePossessedByPlayer(InPlayerState);
}

APawn* AForgeVehicleGunner::GetSeatedPawn() const
{
	if (IsValid(SeatConfig) && IsValid(SeatConfig->PlayerSeatComponent))
	{
		return Cast<APawn>(SeatConfig->PlayerSeatComponent->GetOwner());
	}

	return nullptr;
}

void AForgeVehicleGunner::Input_MouseLook(const FInputActionValue& Value)
{
	const FVector2D ValueVec = Value.Get<FVector2D>();

	if (ValueVec.X != 0.0)
	{
		AddControllerYawInput(ValueVec.X);
	}

	if (ValueVec.Y != 0.0)
	{
		AddControllerPitchInput(ValueVec.Y);
	}
}
