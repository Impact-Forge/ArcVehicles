// Copyright 2017-2020 Puny Human, All Rights Reserved.

#include "ForgeVehiclesCore.h"
#include "ForgeBaseVehicle.h"
#include "ForgeVehicleTypes.h"
#include "Player/ForgeVehiclePlayerSeatComponent.h"
#include "ForgeVehicleSeatConfig.h"
#include "ForgeVehicleEngineSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "EngineMinimal.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"

#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "DisplayDebugHelpers.h"


// Sets default values for this component's properties
UForgeVehiclePlayerSeatComponent::UForgeVehiclePlayerSeatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	RelativeTransformRestorer = FForgeVehicleScopedRelativeTransformRestoration(nullptr);
	// ...
}


void UForgeVehiclePlayerSeatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UForgeVehiclePlayerSeatComponent, CurrentSeatConfig, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UForgeVehiclePlayerSeatComponent, StoredPlayerState, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UForgeVehiclePlayerSeatComponent, ServerDebugStrings, COND_None, REPNOTIFY_Always);
}

// Called when the game starts
void UForgeVehiclePlayerSeatComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...

}

void UForgeVehiclePlayerSeatComponent::OnRegister()
{
	Super::OnRegister();

	if (GetOwnerRole() == ROLE_Authority)
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			StoredPlayerState = OwnerPawn->GetPlayerState();
		}
	}

	RelativeTransformRestorer = FForgeVehicleScopedRelativeTransformRestoration(GetOwner());
}

void UForgeVehiclePlayerSeatComponent::ChangeSeats(const FForgeVehicleSeatReference& NewSeat)
{
	PreviousSeatConfig = CurrentSeatConfig;
	CurrentSeatConfig = NewSeat;

	EForgeVehicleSeatChangeType SeatChangeType = EForgeVehicleSeatChangeType::Invalid;

	//We've entered a vehicle
	if (!PreviousSeatConfig.IsValid() && CurrentSeatConfig.IsValid())
	{
		SeatChangeType = EForgeVehicleSeatChangeType::EnterVehicle;

	}
	//We've swapped seats
	if (PreviousSeatConfig.IsValid() && CurrentSeatConfig.IsValid())
	{
		SeatChangeType = EForgeVehicleSeatChangeType::SwitchSeats;
	}
	//We've exited the vehicle
	if (PreviousSeatConfig.IsValid() && !CurrentSeatConfig.IsValid())
	{
		SeatChangeType = EForgeVehicleSeatChangeType::ExitVehicle;
	}
	//There is no 4th case (when both seats are nullptr.  That shouldn't happen.  If it does... ignore it.

	OnSeatChangeEvent(SeatChangeType);

	//Inform the vehicle of this seat change event on both client and server

	AForgeBaseVehicle* Vehicle = nullptr;
	if (CurrentSeatConfig.IsValid())
	{
		Vehicle = CurrentSeatConfig->GetVehicleOwner();
	}
	else if (PreviousSeatConfig.IsValid())
	{
		Vehicle = PreviousSeatConfig->GetVehicleOwner();
	}

	if (IsValid(Vehicle))
	{
		Vehicle->NotifyPlayerSeatChangeEvent(StoredPlayerState, *CurrentSeatConfig, *PreviousSeatConfig, SeatChangeType);
	}

	DebugLastSeatChangeType = SeatChangeType;
}

void UForgeVehiclePlayerSeatComponent::OnRep_SeatConfig(const FForgeVehicleSeatReference& InPreviousSeatConfig)
{
	//So, we reverted the replication here because ChangeSeats stores the previous seat and changes SeatConfig itself.
	FForgeVehicleSeatReference CurrentSeat = CurrentSeatConfig;
	CurrentSeatConfig = InPreviousSeatConfig;

	//Make sure that on the client, we know the seat is ours.  This isn't replicated from the server but we can derive it so bandwidth savings.
	if (CurrentSeat.IsValid())
	{
		CurrentSeat->PlayerSeatComponent = this;
		CurrentSeat->PlayerInSeat = StoredPlayerState;
	}

	ChangeSeats(CurrentSeat);

	//Clean out the player seat component on the client.  The seat change code handles this on the server
	if (PreviousSeatConfig.IsValid())
	{
		PreviousSeatConfig->PlayerInSeat = nullptr;
		PreviousSeatConfig->PlayerSeatComponent = nullptr;
	}
}

void UForgeVehiclePlayerSeatComponent::OnRep_StoredPlayerState(APlayerState* InPreviousPlayerState)
{
	BP_OnRep_StoredPlayerState(InPreviousPlayerState);
}


void UForgeVehiclePlayerSeatComponent::OnSeatChangeEvent_Implementation(EForgeVehicleSeatChangeType SeatChangeType)
{
	if (PreviousSeatConfig.IsValid())
	{
		PreviousSeatConfig->UnAttachPlayerFromSeat(StoredPlayerState);
	}

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (SeatChangeType == EForgeVehicleSeatChangeType::EnterVehicle || SeatChangeType == EForgeVehicleSeatChangeType::SwitchSeats)
		{
			if (CurrentSeatConfig.IsValid())
			{
				SetIgnoreBetween(CurrentSeatConfig->GetVehicleOwner());

				{
					CurrentSeatConfig->AttachPlayerToSeat(StoredPlayerState);
				}

				if (ACharacter* OwnerChar = Cast<ACharacter>(OwnerPawn))
				{
					if (GetOwnerRole() == ROLE_Authority)
					{
						OwnerChar->GetCharacterMovement()->FlushServerMoves();
						OwnerChar->ForceNetUpdate();
						OwnerChar->GetCharacterMovement()->ForceReplicationUpdate();
						OwnerChar->GetCharacterMovement()->ForceClientAdjustment();
					}
					OwnerChar->GetCharacterMovement()->StopMovementImmediately();
					OwnerChar->GetCharacterMovement()->DisableMovement();
					OwnerChar->GetCharacterMovement()->SetComponentTickEnabled(false);

				}
			}
		}

		if (SeatChangeType == EForgeVehicleSeatChangeType::ExitVehicle)
		{
			//Enable player movement
			if (ACharacter* OwnerChar = Cast<ACharacter>(OwnerPawn))
			{
				OwnerChar->GetCharacterMovement()->StopMovementImmediately();
				OwnerChar->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				OwnerChar->GetCharacterMovement()->SetComponentTickEnabled(true);
			}


			//Find them an exit point.  This has to be done after we re-enable movement otherwise we don't get teleported
			//if (GetOwnerRole() == ROLE_Authority)
			{
				FVector ExitLoc = GetOwner()->GetActorLocation() + FVector(0, 0, 300);
				if (PreviousSeatConfig.IsValid())
				{
					TArray<FTransform> ExitLocations;
					PreviousSeatConfig->GetVehicleOwner()->GetSortedExitPoints(GetOwner()->GetActorTransform(), ExitLocations);

					if (ExitLocations.Num() > 0)
					{		
						for (const FTransform& TestLocation : ExitLocations)
						{
							FVector TestLocVec = TestLocation.GetLocation();
							if (GetWorld()->FindTeleportSpot(GetOwner(), TestLocVec, TestLocation.GetRotation().Rotator()))
							{
								ExitLoc = TestLocVec;
								break;
							}							
						}
					}
				}

				OwnerPawn->SetActorLocationAndRotation(ExitLoc, FQuat::Identity, false, nullptr, ETeleportType::TeleportPhysics);
			}

			//Force the player to right itself
			OwnerPawn->SetActorRotation(FQuat::Identity);

			{
				//Reset the player.  If they are invisible, make them visible
				FDetachmentTransformRules DetachmentRules(EDetachmentRule::KeepWorld, true);
				OwnerPawn->DetachFromActor(DetachmentRules);
				OwnerPawn->SetActorHiddenInGame(false);
			}

			//Allow them to collide with everything
			UForgeVehicleEngineSubsystem* EngSub = GEngine->GetEngineSubsystem<UForgeVehicleEngineSubsystem>();
			TInlineComponentArray<UPrimitiveComponent*> VehicleComponents(PreviousSeatConfig->GetVehicleOwner());
			TInlineComponentArray<UPrimitiveComponent*> OwnerPawnComponents(OwnerPawn);

			for (UPrimitiveComponent* VC : VehicleComponents)
			{
				for (UPrimitiveComponent* SC : OwnerPawnComponents)
				{
					EngSub->RemoveIgnoreBetween(VC, SC);
				}
			}
		}
	}

	RelativeTransformRestorer.Restore();
}

void UForgeVehiclePlayerSeatComponent::SetIgnoreBetween(AActor* OtherActor)
{
	UForgeVehicleEngineSubsystem* EngSub = GEngine->GetEngineSubsystem<UForgeVehicleEngineSubsystem>();
	TInlineComponentArray<UPrimitiveComponent*> VehicleComponents(OtherActor);
	TInlineComponentArray<UPrimitiveComponent*> OwnerPawnComponents(GetOwner());

	for (UPrimitiveComponent* VC : VehicleComponents)
	{
		for (UPrimitiveComponent* SC : OwnerPawnComponents)
		{
			EngSub->IgnoreBetween(VC, SC);
		}
	}
}

namespace ForgeVehiclesCoreSeatDebug
{
	struct FDebugTargetInfo
	{
		FDebugTargetInfo()
		{

		}

		TWeakObjectPtr<UWorld> TargetWorld;
		TWeakObjectPtr<UForgeVehiclePlayerSeatComponent> LastDebugTarget;
	};

	TArray<FDebugTargetInfo> InventoryDebugInfoList;

	FDebugTargetInfo* GetDebugTargetInfo(UWorld* World)
	{
		FDebugTargetInfo* TargetInfo = nullptr;
		for (FDebugTargetInfo& Info : InventoryDebugInfoList)
		{
			if (Info.TargetWorld.Get() == World)
			{
				TargetInfo = &Info;
				break;
			}
		}
		if (TargetInfo == nullptr)
		{
			TargetInfo = &InventoryDebugInfoList[InventoryDebugInfoList.AddDefaulted()];
			TargetInfo->TargetWorld = World;
		}

		return TargetInfo;
	}

	UForgeVehiclePlayerSeatComponent* GetDebugTarget(FDebugTargetInfo* TargetInfo)
	{
		//Return the Target if we have one
		if (UForgeVehiclePlayerSeatComponent* Inv = TargetInfo->LastDebugTarget.Get())
		{
			return Inv;
		}

		//Find one
		for (TObjectIterator<UForgeVehiclePlayerSeatComponent> It; It; ++It)
		{
			if (UForgeVehiclePlayerSeatComponent* Inv = *It)
			{
				if (Inv->GetWorld() == TargetInfo->TargetWorld.Get() && MakeWeakObjectPtr(Inv).Get())
				{
					TargetInfo->LastDebugTarget = Inv;

					//Default to local player
					if (APawn* Pawn = Cast<APawn>(Inv->GetOwner()))
					{
						if (Pawn->IsLocallyControlled())
						{
							break;
						}
					}
				}
			}
		}
		return TargetInfo->LastDebugTarget.Get();
	}

	FString PrintDebugSeatInfo(UForgeVehicleSeatConfig* SeatConfig)
	{
		if (!IsValid(SeatConfig))
		{
			return TEXT("null");
		}

		return FString::Printf(TEXT("Seat (%s) Owner(%s), IsDriver(%s) PlayerInSeat (%s), PlayerCompInSeat (%s) Vehicle (%s) VehicleOwner (%s)"),
			*SeatConfig->GetClass()->GetAuthoredName(),
			*SeatConfig->GetVehicleOwner()->GetName(),
			(SeatConfig->GetVehicleOwner()->GetDriverSeat() == SeatConfig) ? TEXT("true") : TEXT("false"),
			IsValid(SeatConfig->PlayerInSeat) ? *SeatConfig->PlayerInSeat->GetPlayerName() : TEXT("null"),
			IsValid(SeatConfig->PlayerSeatComponent) ? *SeatConfig->PlayerSeatComponent->GetName() : TEXT("null"),
			IsValid(SeatConfig->GetVehicleOwner()) ? *SeatConfig->GetVehicleOwner()->GetName() : TEXT("null"),
			IsValid(SeatConfig->GetVehicleOwner()->GetOwner()) ? *SeatConfig->GetVehicleOwner()->GetOwner()->GetName() : TEXT("null")
		);
	}

	void CycleDebugTarget(FDebugTargetInfo* TargetInfo, bool Next)
	{
		GetDebugTarget(TargetInfo);

		// Build a list	of ASCs
		TArray<UForgeVehiclePlayerSeatComponent*> List;
		for (TObjectIterator<UForgeVehiclePlayerSeatComponent> It; It; ++It)
		{
			if (UForgeVehiclePlayerSeatComponent* SeatComp = *It)
			{
				if (SeatComp->GetWorld() == TargetInfo->TargetWorld.Get())
				{
					List.Add(SeatComp);
				}
			}
		}

		if (List.Num() == 0)
		{
			return;
		}

		// Search through list to find prev/next target
		UForgeVehiclePlayerSeatComponent* Previous = nullptr;
		for (int32 idx = 0; idx < List.Num() + 1; ++idx)
		{
			UForgeVehiclePlayerSeatComponent* SeatComp = List[idx % List.Num()];

			if (Next && Previous == TargetInfo->LastDebugTarget.Get())
			{
				TargetInfo->LastDebugTarget = SeatComp;
				return;
			}
			if (!Next && SeatComp == TargetInfo->LastDebugTarget.Get())
			{
				TargetInfo->LastDebugTarget = Previous;
				return;
			}

			Previous = SeatComp;
		}
	}

	static void	VehicleCycleDebugTarget(UWorld* InWorld, bool Next)
	{
		CycleDebugTarget(GetDebugTargetInfo(InWorld), Next);
	}

	FAutoConsoleCommandWithWorld VehicleSeatNextDebugTargetCmd(
		TEXT("ForgeVehicleSeat.Debug.NextTarget"),
		TEXT("Targets next PlayerSeat in ShowDebug VehicleSeat"),
		FConsoleCommandWithWorldDelegate::CreateStatic(VehicleCycleDebugTarget, true)
	);

	FAutoConsoleCommandWithWorld VehicleSeatPrevDebugTargetCmd(
		TEXT("ForgeVehicleSeat.Debug.PrevTarget"),
		TEXT("Targets previous PlayerSeat in ShowDebug VehicleSeat"),
		FConsoleCommandWithWorldDelegate::CreateStatic(VehicleCycleDebugTarget, false)
	);

	FDelegateHandle DebugHandle = AHUD::OnShowDebugInfo.AddStatic(&UForgeVehiclePlayerSeatComponent::OnShowDebugInfo);
}

void UForgeVehiclePlayerSeatComponent::OnShowDebugInfo(class AHUD* HUD, class UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	if (DisplayInfo.IsDisplayOn(TEXT("VehicleSeat")))
	{
		UWorld* World = HUD->GetWorld();
		ForgeVehiclesCoreSeatDebug::FDebugTargetInfo* TargetInfo = ForgeVehiclesCoreSeatDebug::GetDebugTargetInfo(World);

		if (UForgeVehiclePlayerSeatComponent* Comp = ForgeVehiclesCoreSeatDebug::GetDebugTarget(TargetInfo))
		{
			TArray<FName> LocalDisplayNames;
			LocalDisplayNames.Add(TEXT("CVehicleSeat"));
			FDebugDisplayInfo LocalDisplayInfo(LocalDisplayNames, TArray<FName>());

			Comp->DisplayDebug(Canvas, LocalDisplayInfo, YL, YPos);
		}
	}
}

void UForgeVehiclePlayerSeatComponent::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	if (DebugDisplay.IsDisplayOn(TEXT("CVehicleSeat")))
	{
		FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
		TArray<FString> ClientStrings;
		GenerateDebugStrings(ClientStrings);

		DisplayDebugManager.DrawString(TEXT("---CLIENT SEAT STATE---"));
		for (const FString& String : ClientStrings)
		{
			DisplayDebugManager.DrawString(String, 15);
		}

		DisplayDebugManager.DrawString(TEXT("---SERVER SEAT STATE---"));
		for (const FString& String : ServerDebugStrings)
		{
			DisplayDebugManager.DrawString(String, 15);
		}

		//Ask for the server strings.  This is very chatty, but it's debug.
		if (GetOwnerRole() != ROLE_Authority && ShouldRequestDebugStrings())
		{
			ServerPrintDebug_Request();
		}

	}
}

void UForgeVehiclePlayerSeatComponent::GenerateDebugStrings(TArray<FString>& OutStrings)
{
	OutStrings.Add(FString::Printf(TEXT("Current Seat: %s"), *ForgeVehiclesCoreSeatDebug::PrintDebugSeatInfo(*CurrentSeatConfig)));
	OutStrings.Add(FString::Printf(TEXT("Previous Seat: %s"), *ForgeVehiclesCoreSeatDebug::PrintDebugSeatInfo(*PreviousSeatConfig)));

	OutStrings.Add(FString::Printf(TEXT("Last Seat Change Event: %s"), *UEnum::GetValueAsString(DebugLastSeatChangeType)));

	OutStrings.Add(FString::Printf(TEXT("PlayerPawn: NetOwner (%s)"),
		IsValid(GetOwner()->GetOwner()) ? *GetOwner()->GetOwner()->GetName() : TEXT("null")
	));
}

void UForgeVehiclePlayerSeatComponent::ServerPrintDebug_Request_Implementation()
{
	ServerDebugStrings.Empty(ServerDebugStrings.Num());
	GenerateDebugStrings(ServerDebugStrings);
}

bool UForgeVehiclePlayerSeatComponent::ServerPrintDebug_Request_Validate()
{
	return true;
}

void UForgeVehiclePlayerSeatComponent::OnRep_ServerDebugStrings()
{

}

bool UForgeVehiclePlayerSeatComponent::ShouldRequestDebugStrings() const
{
	// This implements basic throttling so that debug strings can't be sent more than once a second to the server
	const double MinTimeBetweenClientDebugSends = 1.f;
	static double LastSendTime = 0.f;

	double CurrentTime = FPlatformTime::Seconds();
	bool ShouldSend = (CurrentTime - LastSendTime) > MinTimeBetweenClientDebugSends;
	if (ShouldSend)
	{
		LastSendTime = CurrentTime;
	}
	return ShouldSend;
}


