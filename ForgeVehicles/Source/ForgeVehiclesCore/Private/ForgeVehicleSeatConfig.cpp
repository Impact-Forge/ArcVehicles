// Copyright 2017-2020 Puny Human, All Rights Reserved.

#include "ForgeVehiclesCore.h"
#include "ForgeVehicleSeatConfig.h"
#include "ForgeBaseVehicle.h"
#include "Player/ForgeVehiclePlayerSeatComponent.h"
#include "Seats/ForgeVehicleSeat.h"
#include "ForgeVehicleEngineSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "EngineMinimal.h"
#include "Engine.h"



UForgeVehicleSeatConfig::UForgeVehicleSeatConfig()
	: Super()
{
	
}

void UForgeVehicleSeatConfig::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UForgeVehicleSeatConfig, AttachSeatToComponent);
	DOREPLIFETIME(UForgeVehicleSeatConfig, bPlayerVisible);
}

bool UForgeVehicleSeatConfig::IsOpenSeat() const
{
	return !IsValid(PlayerInSeat);
}

class AForgeBaseVehicle* UForgeVehicleSeatConfig::GetVehicleOwner() const
{
	return Cast<AForgeBaseVehicle>(GetOuter());
}

void UForgeVehicleSeatConfig::SetupSeatAttachment_Implementation()
{

}

void UForgeVehicleSeatConfig::AttachPlayerToSeat(APlayerState* Player)
{
	PlayerInSeat = Player;

	if (IsValid(PlayerSeatComponent))
	{
		if (APawn* PlayerPawn = Cast<APawn>(PlayerSeatComponent->GetOwner()))
		{

			USceneComponent* SceneComponent = AttachSeatToComponent.GetSceneComponent(GetVehicleOwner());
			if (IsValid(SceneComponent))
			{
				FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
				PlayerPawn->AttachToComponent(SceneComponent, AttachmentRules, AttachSeatToComponent.SocketName);

				PlayerPawn->SetActorHiddenInGame(!bPlayerVisible);
			}
			else
			{
				PlayerPawn->AttachToActor(GetVehicleOwner(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				PlayerPawn->SetActorHiddenInGame(true); //If we are always attaching to the actor, just hide us
			}

		}
	}

	BP_AttachPlayerToSeat(Player);
}

void UForgeVehicleSeatConfig::UnAttachPlayerFromSeat(APlayerState* Player)
{
	BP_UnAttachPlayerFromSeat(Player);
	PlayerInSeat = nullptr;
	PlayerSeatComponent = nullptr;
}

AForgeVehiclePawn* UForgeVehicleSeatConfig::GetSeatPawn() const
{
	if (IsDriverSeat())
	{
		return GetVehicleOwner();
	}

	return nullptr;
}

FTransform UForgeVehicleSeatConfig::GetSeatAttachTransform_World()
{
	USceneComponent* SceneComponent = AttachSeatToComponent.GetSceneComponent(GetVehicleOwner());
	if (IsValid(SceneComponent))
	{
		return SceneComponent->GetSocketTransform(AttachSeatToComponent.SocketName, RTS_World);
	}
	else
	{
		return GetVehicleOwner()->GetActorTransform();
	}
}

FTransform UForgeVehicleSeatConfig::GetSawnAttachTrasnform_Relative()
{
	USceneComponent* SceneComponent = AttachSeatToComponent.GetSceneComponent(GetVehicleOwner());
	if (IsValid(SceneComponent))
	{
		return SceneComponent->GetSocketTransform(AttachSeatToComponent.SocketName, RTS_Actor);
	}
	else
	{
		return GetVehicleOwner()->GetActorTransform();
	}
}

bool UForgeVehicleSeatConfig::IsSupportedForNetworking() const
{
	return true;
}

bool UForgeVehicleSeatConfig::IsDriverSeat() const
{
	return GetVehicleOwner()->GetDriverSeat() == this;
}

void UForgeVehicleSeatConfig_PlayerAttachment::AttachPlayerToSeat(APlayerState* Player)
{
	Super::AttachPlayerToSeat(Player);
}

UForgeVehicleSeatConfig_SeatPawn::UForgeVehicleSeatConfig_SeatPawn()
	: Super()
{
	bResetControlRotationOnEnter = true;
}

void UForgeVehicleSeatConfig_SeatPawn::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UForgeVehicleSeatConfig_SeatPawn, SeatPawn);
	//DOREPLIFETIME(UForgeVehicleSeatConfig_SeatPawn, SeatPawnClass);
	DOREPLIFETIME(UForgeVehicleSeatConfig_SeatPawn, PlayerCharacterAttachToComponent);
	DOREPLIFETIME(UForgeVehicleSeatConfig_SeatPawn, bResetControlRotationOnEnter);
}

void UForgeVehicleSeatConfig_SeatPawn::OnRep_SeatPawn(AForgeVehiclePawn* OldSeatPawn)
{
	if (!IsValid(SeatPawn))
	{
		return;
	}

	AForgeBaseVehicle* OwnerVehicle = GetVehicleOwner();
	check(IsValid(OwnerVehicle));


	UForgeVehicleEngineSubsystem* EngSub = GEngine->GetEngineSubsystem<UForgeVehicleEngineSubsystem>();
	TInlineComponentArray<UPrimitiveComponent*> VehicleComponents(OwnerVehicle);
	TInlineComponentArray<UPrimitiveComponent*> SeatPawnComponents(SeatPawn);

	for (UPrimitiveComponent* VC : VehicleComponents)
	{
		for (UPrimitiveComponent* SC : SeatPawnComponents)
		{
			EngSub->IgnoreBetween(VC, SC);
		}
	}
}

void UForgeVehicleSeatConfig_SeatPawn::SetupSeatAttachment_Implementation()
{
	Super::SetupSeatAttachment_Implementation();

	//Don't spawn the seat if we are the driver seat
	if (IsDriverSeat())
	{
		return;
	}

	if (!ensure(IsValid(SeatPawnClass)))
	{
		return;
	}

	AForgeBaseVehicle* OwnerVehicle = GetVehicleOwner();
	check(IsValid(OwnerVehicle));

	USceneComponent* SC = AttachSeatToComponent.GetSceneComponent(OwnerVehicle);
	FTransform TForm = FTransform::Identity;
	if (IsValid(SC))
	{
		TForm = SC->GetSocketTransform(AttachSeatToComponent.SocketName, RTS_World);
	}
	else
	{
		TForm = OwnerVehicle->GetActorTransform();
	}

	AForgeVehicleSeat* NewSeatPawn = OwnerVehicle->GetWorld()->SpawnActorDeferred<AForgeVehicleSeat>(SeatPawnClass, TForm, OwnerVehicle, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (IsValid(NewSeatPawn))
	{
		NewSeatPawn->SeatConfig = this;

		UGameplayStatics::FinishSpawningActor(NewSeatPawn, TForm);
	}
	else
	{
		return;
	}


	SeatPawn = NewSeatPawn;
	if (IsValid(SeatPawn))
	{
		if (IsValid(SC))
		{

			SeatPawn->AttachToComponent(SC, FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSeatToComponent.SocketName);
		}
		else
		{
			SeatPawn->AttachToActor(OwnerVehicle, FAttachmentTransformRules::SnapToTargetIncludingScale);
		}
	}

	OnRep_SeatPawn(nullptr);
}

void UForgeVehicleSeatConfig_SeatPawn::AttachPlayerToSeat(APlayerState* Player)
{
	Super::AttachPlayerToSeat(Player);
	if (bResetControlRotationOnEnter)
	{
		if (AController* Controller = Cast<AController>(Player->GetPawn()->GetController()))
		{
			Controller->SetControlRotation(FRotator::ZeroRotator);
		}
	}
}

AForgeVehiclePawn* UForgeVehicleSeatConfig_SeatPawn::GetSeatPawn() const
{
	if (!IsValid(SeatPawn))
	{
		return Super::GetSeatPawn();
	}

	return SeatPawn;
}
