// Copyright 2017-2020 Puny Human, All Rights Reserved.

#include "ForgeVehiclesCore.h"
#include "ForgeBaseVehicle.h"
#include "EngineMinimal.h"
#include "ForgeVehicleSeatConfig.h"
#include "GameFramework/PlayerState.h"
#include "Player/ForgeVehiclePlayerSeatComponent.h"
#include "Player/ForgeVehiclePlayerStateComponent.h"
#include "ForgeVehicleDeveloperSettings.h"

#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h" 
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Net/RepLayout.h"

#include "ForgeVehicleExitPoint.h"
#include "Engine/GameInstance.h"

#if FORGEVEHICLES_MODULAR
#include "Components/GameFrameworkComponentManager.h"
#endif

int32 FForgeVehicleSeatChangeEvent::NO_SEAT = INDEX_NONE;
int32 FForgeVehicleSeatChangeEvent::ANY_SEAT = INT32_MAX;


// Sets default values
AForgeBaseVehicle::AForgeBaseVehicle()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AForgeBaseVehicle::PostInitProperties()
{
	Super::PostInitProperties();	
}

void AForgeBaseVehicle::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//HACKHACK: Due to oddities in how Unreal Engine replicates instanced subobjects
	//we duplicate them here.  The replicated array then syncs the objects safely to the client
	if (!IsNetMode(NM_Standalone))
	{
		ReplicatedSeatConfigs.Empty(1 + AdditionalSeatConfigs.Num());

		if (GetLocalRole() == ROLE_Authority)
		{
			UForgeVehicleSeatConfig* DupSeatConfig = DuplicateObject(DriverSeatConfig, this);
			if (IsValid(DupSeatConfig))
			{
				DupSeatConfig->ClearFlags(RF_ArchetypeObject | RF_WasLoaded);
				ReplicatedSeatConfigs.Insert(DupSeatConfig, 0);
			}

			for (int32 i = 0; i < AdditionalSeatConfigs.Num(); i++)
			{
				DupSeatConfig = DuplicateObject(AdditionalSeatConfigs[i], this);
				DupSeatConfig->ClearFlags(RF_ArchetypeObject | RF_WasLoaded);			
				ReplicatedSeatConfigs.Add(DupSeatConfig);
			}
		}
	}
}

void AForgeBaseVehicle::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//DOREPLIFETIME(AForgeBaseVehicle, DriverSeatConfig);
	//DOREPLIFETIME(AForgeBaseVehicle, AdditionalSeatConfigs);
	DOREPLIFETIME(AForgeBaseVehicle, ReplicatedSeatConfigs);

	DOREPLIFETIME_CONDITION_NOTIFY(AForgeBaseVehicle, ServerDebugStrings, COND_None, REPNOTIFY_Always);
}

bool AForgeBaseVehicle::ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	
	TArray<UForgeVehicleSeatConfig*> AllSeats;
	GetAllSeats(AllSeats);

	for (UForgeVehicleSeatConfig* SeatConfig : AllSeats)
	{
		bWroteSomething |= Channel->ReplicateSubobject(SeatConfig, *Bunch, *RepFlags);		
	}
	
	return bWroteSomething;
}

void AForgeBaseVehicle::PostNetReceivePhysicState()
{
	//Ensure that the seat ignores are set up.
	UpdatePhysicsIgnores();

	Super::PostNetReceivePhysicState();
}

// Called when the game starts or when spawned
void AForgeBaseVehicle::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SetupVehicleSeats();
	}
	
}

// Called every frame
void AForgeBaseVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetLocalRole() == ROLE_Authority)
	{
		ProcessSeatChangeQueue();
	}

}

void AForgeBaseVehicle::Restart()
{
	Super::Restart();
	
}

// Called to bind functionality to input
void AForgeBaseVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

UForgeVehicleSeatConfig* AForgeBaseVehicle::GetSeatConfig()
{
	return GetDriverSeat();
}

UForgeVehicleSeatConfig* AForgeBaseVehicle::GetSeatConfig(const FForgeVehicleSeatReference& SeatRef)
{
	if (SeatRef.Vehicle != this)
	{
		return nullptr;
	}
	if (!IsValidSeatIndex(SeatRef.SeatID))
	{
		return nullptr;
	}

	TArray<UForgeVehicleSeatConfig*> SeatConfigs;
	GetAllSeats(SeatConfigs);

	return SeatConfigs[SeatRef.SeatID];
}

UForgeVehicleSeatConfig* AForgeBaseVehicle::GetDriverSeat()
{
	if (ReplicatedSeatConfigs.Num() > 0)
	{
		return ReplicatedSeatConfigs[0];
	}

	return DriverSeatConfig;
}

AForgeBaseVehicle* AForgeBaseVehicle::GetOwningVehicle() 
{
	return this;
}

void AForgeBaseVehicle::SetupVehicleSeats()
{
	TArray<UForgeVehicleSeatConfig*> Seats;
	GetAllSeats(Seats);

	for (UForgeVehicleSeatConfig* SeatConfig : Seats)
	{
		if (SeatConfig == GetDriverSeat())
		{
			if (UForgeVehicleSeatConfig_SeatPawn* PawnConfig = Cast<UForgeVehicleSeatConfig_SeatPawn>(SeatConfig))
			{
				PawnConfig->SeatPawnClass = nullptr;
				PawnConfig->SeatPawn = this;
			}
		}

		SetupSeat(SeatConfig);
	}
}

void AForgeBaseVehicle::SetupSeat_Implementation(UForgeVehicleSeatConfig* SeatConfig)
{
	if (IsValid(SeatConfig))
	{
		SeatConfig->SetupSeatAttachment();
	}
}

void AForgeBaseVehicle::GetAllSeats(TArray<UForgeVehicleSeatConfig*>& Seats)
{
	if (ReplicatedSeatConfigs.Num() > 0)
	{
		Seats.Reset(ReplicatedSeatConfigs.Num());
		Seats.Append(ReplicatedSeatConfigs);
	}
	else
	{
		Seats.Reset(AdditionalSeatConfigs.Num() + 1);
		Seats.Add(DriverSeatConfig);
		Seats.Append(AdditionalSeatConfigs);
	}	
}

bool AForgeBaseVehicle::CanProcessSeatChange(const FForgeVehicleSeatChangeEvent& SeatChange)
{
	return true;
}

UForgeVehicleSeatConfig* AForgeBaseVehicle::FindSeatContainingPlayer(APlayerState* Player)
{
	TArray<UForgeVehicleSeatConfig*> AllSeats;
	GetAllSeats(AllSeats);

	for (UForgeVehicleSeatConfig* Config : AllSeats)
	{
		// Seat arrays can legitimately contain nulls: a vehicle may declare no driver seat at all
		// (unmanned platforms), or have unassigned additional-seat entries on its Blueprint.
		if (IsValid(Config) && Config->PlayerInSeat == Player)
		{
			return Config;
		}
	}

	return nullptr;
}

void AForgeBaseVehicle::RequestEnterAnySeat(APlayerState* InPlayerState)
{
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	FForgeVehicleSeatChangeEvent SeatChangeEvent;
	SeatChangeEvent.Player = InPlayerState;
	SeatChangeEvent.FromSeat = FForgeVehicleSeatChangeEvent::NO_SEAT;
	SeatChangeEvent.ToSeat = FForgeVehicleSeatChangeEvent::ANY_SEAT;
	SeatChangeEvent.bFindEmptySeatOnFail = true;

	PushSeatChangeEvent(SeatChangeEvent);
}

void AForgeBaseVehicle::RequestLeaveVehicle(APlayerState* InPlayerState)
{
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	FForgeVehicleSeatChangeEvent SeatChangeEvent;
	SeatChangeEvent.Player = InPlayerState;
	SeatChangeEvent.FromSeat = FForgeVehicleSeatChangeEvent::ANY_SEAT;
	SeatChangeEvent.ToSeat = FForgeVehicleSeatChangeEvent::NO_SEAT;

	PushSeatChangeEvent(SeatChangeEvent);
}

void AForgeBaseVehicle::RequestEnterSeat(APlayerState* InPlayerState, int32 RequestedSeat, bool bIgnoreRestrictions /*= false*/)
{
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	//If we don't have a valid seat, return
	if (!IsValidSeatIndex(RequestedSeat))
	{
		return;
	}
	

	FForgeVehicleSeatChangeEvent SeatChangeEvent;
	SeatChangeEvent.Player = InPlayerState;
	SeatChangeEvent.FromSeat = FForgeVehicleSeatChangeEvent::ANY_SEAT;
	SeatChangeEvent.ToSeat = RequestedSeat;
	SeatChangeEvent.bIgnoreAnyRestrictions = bIgnoreRestrictions;

	PushSeatChangeEvent(SeatChangeEvent);
}

bool AForgeBaseVehicle::IsValidSeatIndex(int32 InSeatIndex) const
{
	if (InSeatIndex < 0)
	{
		return false;
	}

	if (InSeatIndex >= AdditionalSeatConfigs.Num() + 1)
	{
		return false;
	}

	return true;
}

FTransform AForgeBaseVehicle::GetNearestExitTransform_Implementation(FTransform InputLocation)
{
	//Get all of the exit positions
	TArray<FTransform> ExitLocations;
	GetSortedExitPoints(InputLocation, ExitLocations);

	if (ExitLocations.Num() > 0)
	{
		return ExitLocations[0];
	}
	else
	{
		FVector ExitLoc = GetActorLocation() + FVector(0, 0, 300);
		return FTransform(FRotator(), ExitLoc);
	}
}

void AForgeBaseVehicle::GetSortedExitPoints(FTransform InputLocation, TArray<FTransform>& OutTransformArray) const
{
	TInlineComponentArray<UForgeVehicleExitPoint*> ExitComponents(this, true);
	OutTransformArray.Reset(ExitComponents.Num());

	//Get the exit transforms
	for (UForgeVehicleExitPoint* ExitPoint : ExitComponents)
	{
		if (IsValid(ExitPoint))
		{
			OutTransformArray.Add(ExitPoint->GetComponentTransform());
		}
	}

	//Sort the array
	OutTransformArray.Sort([InputLocation](const FTransform& A, const FTransform& B)
		{
			return FVector::DistSquared(A.GetLocation(), InputLocation.GetLocation()) < FVector::DistSquared(B.GetLocation(), InputLocation.GetLocation());
		});
}

void AForgeBaseVehicle::PushSeatChangeEvent(const FForgeVehicleSeatChangeEvent& SeatChangeEvent)
{
	if (CanProcessSeatChange(SeatChangeEvent))
	{
		SeatChangeQueue.Push(SeatChangeEvent);
	}	
}

void AForgeBaseVehicle::GetAllVehicleActors(TArray<AActor*>& VehicleActors)
{
	TArray<UForgeVehicleSeatConfig*> AllSeats;
	GetAllSeats(AllSeats);

	VehicleActors.Reset(AllSeats.Num());
	VehicleActors.Add(this);
	GetAttachedActors(VehicleActors, false);	
}

void AForgeBaseVehicle::NotifyPlayerSeatChangeEvent_Implementation(APlayerState* Player, UForgeVehicleSeatConfig* ToSeat, UForgeVehicleSeatConfig* FromSeat, EForgeVehicleSeatChangeType SeatChangeEvent)
{
	Super::NotifyPlayerSeatChangeEvent_Implementation(Player, ToSeat, FromSeat, SeatChangeEvent);

	if (UForgeVehicleSeatConfig_SeatPawn* ToSeatPawn = Cast<UForgeVehicleSeatConfig_SeatPawn>(ToSeat))
	{
		if (IsValid(ToSeatPawn->SeatPawn) && ToSeatPawn != GetDriverSeat())
		{
			ToSeatPawn->SeatPawn->NotifyPlayerSeatChangeEvent(Player, ToSeat, FromSeat, SeatChangeEvent);
		}
	}

	if (UForgeVehicleSeatConfig_SeatPawn* FromSeatPawn = Cast<UForgeVehicleSeatConfig_SeatPawn>(FromSeat))
	{
		if (IsValid(FromSeatPawn->SeatPawn) && FromSeatPawn != GetDriverSeat())
		{
			FromSeatPawn->SeatPawn->NotifyPlayerSeatChangeEvent(Player, ToSeat, FromSeat, SeatChangeEvent);
		}
	}
}

void AForgeBaseVehicle::ProcessSeatChangeQueue()
{
	TArray<UForgeVehicleSeatConfig*> AllSeats;
	GetAllSeats(AllSeats);

	while (SeatChangeQueue.Num() > 0)
	{
		FForgeVehicleSeatChangeEvent SeatChangeEvent = SeatChangeQueue.Pop(EAllowShrinking::No);
		
		//If we don't have a player, then we have a problem.
		//We can safely just ignore in shipping, but we should let the developer
		//know something went wrong
		if (!ensure(IsValid(SeatChangeEvent.Player)))
		{
			continue;
		}

		//If the controller doesn't have a player pawn, then we also have a problem
		//Once again, we can ignore this case in shipping, but in development we should let the developer know.  
		if (!ensure(IsValid(SeatChangeEvent.Player->GetPawn())))
		{
			continue;
		}

		//PROCESSING SEAT CHANGES
		//Seat Changes are processed in a sequential way.  Firstly, we find the actual seats that the player wants to go to.
		//The seats can be nullptr.  There are fail conditions if seats are nullptr and there is some intent
		//Then we determine intent (eg: FromSeat ANY, ToSeat NOSEAT means the player just wants out of the vehicle)
		//This helps us process the change.  There are two possible actions that can be taken based on the player's intent
		//  First Action is "PUT PLAYER INTO SEAT".  This takes a player (regardless of where they are) and places them directly in the seat.  
		//  Second is "REMOVE PLAYER FROM VEHICLE".  This takes a player (regardless of where they are) and removes them from the seat that they are in.
		//If a player is moving from one seat to another, we take the first action and then clear out their FromSeat reference pointer.  
		

		if (CanProcessSeatChange(SeatChangeEvent))
		{
			//Get the seat objects from this seat change event.  
			//The Event contains Intents, and ToSeat and FromSeat are the actual possible situations.
			FForgeVehicleSeatReference ToSeat;
			FForgeVehicleSeatReference FromSeat;
			if (SeatChangeEvent.FromSeat >= 0)
			{
				//Find the seat that the player is coming from
				if (AllSeats.IsValidIndex(SeatChangeEvent.FromSeat))
				{
					FromSeat = FForgeVehicleSeatReference(this, SeatChangeEvent.FromSeat);

					check(FromSeat.IsValid()); //Seat Must be valid (bad data?)
					
					//If the player isn't the player in the seat, we have a bad data situation
					checkf(FromSeat->PlayerInSeat == SeatChangeEvent.Player, 
						TEXT("AForgeBaseVehicle::ProcessSeatChangeQueue: Got a Seat Change Event where a different player is in FromSeat.  Player changing seats: %s Player In FromSeat: %s"),
					*SeatChangeEvent.Player->GetPlayerName(), FromSeat->PlayerInSeat ? *FromSeat->PlayerInSeat->GetPlayerName() : TEXT("NULLPTR"));
				}
				else if (SeatChangeEvent.FromSeat == FForgeVehicleSeatChangeEvent::ANY_SEAT)
				{
					//If we are coming from any seat, that doesn't make a whole lot of sense.  So we are going to assume the caller doesn't know
					// where the player is.  So, let's find them.  If we don't find them, then we assume that it's the same as they aren't in the vehicle 
					// and want to get in.

					//Finding a player this way is kind of slow.  The caller should know that. 
					FromSeat = FForgeVehicleSeatReference(FindSeatContainingPlayer(SeatChangeEvent.Player));
				}
			}
			
			if (SeatChangeEvent.ToSeat >= 0)
			{
				if (AllSeats.IsValidIndex(SeatChangeEvent.ToSeat))
				{
					ToSeat = FForgeVehicleSeatReference(this, SeatChangeEvent.ToSeat);
					check(ToSeat.IsValid()); //Seat must be valid (bad data?)

					//If IsValid(ToSeat->PlayerInSeat), that means someone is in this seat
					if (IsValid(ToSeat->PlayerInSeat) && SeatChangeEvent.bFindEmptySeatOnFail)
					{
						ToSeat = FForgeVehicleSeatReference(FindSeatContainingPlayer(nullptr));
					}
					else if (IsValid(ToSeat->PlayerInSeat))
					{
						//This is a fail to do something situation.  
						//We need to discover the intent of this change event, then notify the player that it failed.
						ToSeat = FForgeVehicleSeatReference();
					}
				}
				else if (SeatChangeEvent.ToSeat == FForgeVehicleSeatChangeEvent::ANY_SEAT)
				{
					ToSeat = FForgeVehicleSeatReference(FindSeatContainingPlayer(nullptr));
				}
			}



			//PLAYER INFORMATION
			//Player information is stored in two objects, the PlayerStateComponent (attached to a PlayerState) and the PlayerSeat Component.
			//Player State Component stores information for the player state.  What actor the player was previously possessing before changing it for vehicles, etc. It is authority-only
			//Player Seat Component is replicated and is for any pawn that is attached to the vehicle

			//Check if this player has a PlayerStateComp.  If not, create one for them
			UForgeVehiclePlayerStateComponent* PlayerStateComp = SeatChangeEvent.Player->FindComponentByClass<UForgeVehiclePlayerStateComponent>();
			if (!IsValid(PlayerStateComp))
			{
#if FORGEVEHICLES_MODULAR
				if (UGameFrameworkComponentManager* ComponentManager = GetGameInstance()->GetSubsystem<UGameFrameworkComponentManager>())
				{
					ComponentManager->AddReceiver(SeatChangeEvent.Player);
				}

				PlayerStateComp = SeatChangeEvent.Player->FindComponentByClass<UForgeVehiclePlayerStateComponent>();
				if (!IsValid(PlayerStateComp))
#endif
				{
					
					const UForgeVehicleDeveloperSettings* Settings = GetDefault<UForgeVehicleDeveloperSettings>();
					TSubclassOf<UForgeVehiclePlayerStateComponent> StateCompClass = Settings->PlayerStateComponentClass;

					PlayerStateComp = NewObject<UForgeVehiclePlayerStateComponent>(SeatChangeEvent.Player, StateCompClass);
					PlayerStateComp->RegisterComponent();
				}

			}

			check(IsValid(PlayerStateComp));

			APawn* PlayerPawn = SeatChangeEvent.Player->GetPawn();

			//If the pawn is this or it's owner is this, then the player is sitting in the vehicle and possessing one of our pawns
			//So use our old pawn
			if (PlayerPawn == this || PlayerPawn->GetOwner() == this)
			{
				PlayerPawn = PlayerStateComp->StoredPlayerPawn;
			}

			check(IsValid(PlayerPawn));

			UForgeVehiclePlayerSeatComponent* PlayerSeatComponent = PlayerPawn->FindComponentByClass<UForgeVehiclePlayerSeatComponent>();
			if (!IsValid(PlayerSeatComponent))
			{
#if FORGEVEHICLES_MODULAR
				if (UGameFrameworkComponentManager* ComponentManager = GetGameInstance()->GetSubsystem<UGameFrameworkComponentManager>())
				{
					ComponentManager->AddReceiver(SeatChangeEvent.Player);
				}
				PlayerSeatComponent = PlayerPawn->FindComponentByClass<UForgeVehiclePlayerSeatComponent>();
#endif				
				if (!IsValid(PlayerSeatComponent))
				{
					const UForgeVehicleDeveloperSettings* Settings = GetDefault<UForgeVehicleDeveloperSettings>();
					TSubclassOf<UForgeVehiclePlayerSeatComponent> PlayerCompClass = Settings->PlayerSeatComponentClass;

					PlayerSeatComponent = NewObject<UForgeVehiclePlayerSeatComponent>(PlayerPawn, PlayerCompClass);
					PlayerSeatComponent->RegisterComponent();
				}
			}

			check(IsValid(PlayerSeatComponent));


			//Now to derive the Intent from the event, and what we can actually do
			const bool bWantsOut = SeatChangeEvent.ToSeat == FForgeVehicleSeatChangeEvent::NO_SEAT;
			const bool bWantsASeat = SeatChangeEvent.ToSeat >= 0;
			
			if (bWantsASeat)
			{
				if (ToSeat.IsValid())
				{
					if (FromSeat.IsValid())
					{
						//remove the player from this seat.  
						FromSeat->UnAttachPlayerFromSeat(SeatChangeEvent.Player);
					}

					//Put the player into the seat		
					ToSeat->PlayerInSeat = SeatChangeEvent.Player;
					ToSeat->PlayerSeatComponent = PlayerSeatComponent;
					PlayerSeatComponent->ChangeSeats(ToSeat);

					//a seat with a seat pawn (like the driver seat), take the controller from the player state and posses us.
					AForgeVehiclePawn* SeatPawn = ToSeat->GetSeatPawn();
					if (IsValid(SeatPawn))
					{
						SeatPawn->BecomePossessedByPlayer(SeatChangeEvent.Player);
						PlayerSeatComponent->StoredPlayerState = SeatChangeEvent.Player;
						PlayerStateComp->StoredPlayerPawn = PlayerPawn;
					}
					else
					{
						//Make sure the player possesses their own pawn
						if (AController* SeatedPlayerController = Cast<AController>(SeatChangeEvent.Player->GetOwner()))
						{
							if (SeatedPlayerController->GetPawn() != PlayerPawn)
							{
								//Store the pawn so we can restore net ownership after we change possession.
								APawn* PreviouslyOwnedPawn = SeatedPlayerController->GetPawn();

								SeatedPlayerController->Possess(PlayerStateComp->StoredPlayerPawn);

								PreviouslyOwnedPawn->SetOwner(SeatedPlayerController);
								PreviouslyOwnedPawn->ForceNetUpdate();

								PlayerStateComp->StoredPlayerPawn = nullptr;								
							}							
						}
					}
				}
				else
				{
					//FAIL CONDITION.  Player wants a seat, but there is no seat for them (or the seat is taken)
					//TODO: Inform the player.

					continue;
				}
			}

			if (bWantsOut)
			{
				if (FromSeat.IsValid())
				{		
					PlayerSeatComponent->ChangeSeats(nullptr);
					FromSeat->PlayerInSeat = nullptr;
					FromSeat->PlayerSeatComponent = nullptr;

					if (AController* SeatedPlayerController = Cast<AController>(SeatChangeEvent.Player->GetOwner()))
					{
						APawn* PreviouslyOwnedPawn = SeatedPlayerController->GetPawn();

						SeatedPlayerController->Possess(PlayerStateComp->StoredPlayerPawn);

						//Restore ownership of this pawn back to the person who was owning it for replication purposes.
						PreviouslyOwnedPawn->SetOwner(SeatedPlayerController);
						PreviouslyOwnedPawn->ForceNetUpdate();

						PlayerStateComp->StoredPlayerPawn = nullptr;
					}
					
				}
				else
				{
					//FAIL CONDITION.  The player isn't in the vehicle.  For now, we can just continue
					continue;
				}
			}			
			 
		}
	}
}


int32 AForgeBaseVehicle::GetSeatIndex(UForgeVehicleSeatConfig* Seat)
{
	TArray<UForgeVehicleSeatConfig*> AllSeats;
	GetAllSeats(AllSeats);

	for (int32 i = 0; i < AllSeats.Num(); i++)
	{
		if (AllSeats[i] == Seat)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void AForgeBaseVehicle::UpdatePhysicsIgnores()
{
	TArray<UForgeVehicleSeatConfig*> AllSeats;
	GetAllSeats(AllSeats);

	for (UForgeVehicleSeatConfig* SeatConfig : AllSeats)
	{
		if (IsValid(SeatConfig) && IsValid(SeatConfig->PlayerSeatComponent))
		{
			SeatConfig->PlayerSeatComponent->SetIgnoreBetween(this);
		}
	}
}

//Debug Stuff

#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "DisplayDebugHelpers.h"

namespace ForgeVehicleBaseDebug
{
	struct FDebugTargetInfo
	{
		FDebugTargetInfo()
		{

		}

		TWeakObjectPtr<UWorld> TargetWorld;
		TWeakObjectPtr<AForgeBaseVehicle> LastDebugTarget;
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

	AForgeBaseVehicle* GetDebugTarget(FDebugTargetInfo* TargetInfo)
	{
		//Return the Target if we have one
		if (AForgeBaseVehicle* Inv = TargetInfo->LastDebugTarget.Get())
		{
			return Inv;
		}

		//Find one
		for (TObjectIterator<AForgeBaseVehicle> It; It; ++It)
		{
			if (AForgeBaseVehicle* Vehicle = *It)
			{
				if (Vehicle->GetWorld() == TargetInfo->TargetWorld.Get() && MakeWeakObjectPtr(Vehicle).Get())
				{
					TargetInfo->LastDebugTarget = Vehicle;

					//Default to local player					
					if (Vehicle->IsLocallyControlled())
					{
						break;
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
		TArray<AForgeBaseVehicle*> List;
		for (TObjectIterator<AForgeBaseVehicle> It; It; ++It)
		{
			if (AForgeBaseVehicle* SeatComp = *It)
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
		AForgeBaseVehicle* Previous = nullptr;
		for (int32 idx = 0; idx < List.Num() + 1; ++idx)
		{
			AForgeBaseVehicle* SeatComp = List[idx % List.Num()];

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
		TEXT("ForgeVehicle.Debug.NextTarget"),
		TEXT("Targets next PlayerSeat in ShowDebug Vehicle"),
		FConsoleCommandWithWorldDelegate::CreateStatic(VehicleCycleDebugTarget, true)
	);

	FAutoConsoleCommandWithWorld VehicleSeatPrevDebugTargetCmd(
		TEXT("ForgeVehicle.Debug.PrevTarget"),
		TEXT("Targets previous PlayerSeat in ShowDebug Vehicle"),
		FConsoleCommandWithWorldDelegate::CreateStatic(VehicleCycleDebugTarget, false)
	);

	FDelegateHandle DebugHandle = AHUD::OnShowDebugInfo.AddStatic(&AForgeBaseVehicle::OnShowDebugInfo);
}



void AForgeBaseVehicle::OnShowDebugInfo(class AHUD* HUD, class UCanvas* Canvas, const class FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	if (DisplayInfo.IsDisplayOn(TEXT("ForgeVehicle")))
	{
		UWorld* World = HUD->GetWorld();
		ForgeVehicleBaseDebug::FDebugTargetInfo* TargetInfo = ForgeVehicleBaseDebug::GetDebugTargetInfo(World);

		if (AForgeBaseVehicle* Vehicle = ForgeVehicleBaseDebug::GetDebugTarget(TargetInfo))
		{
			TArray<FName> LocalDisplayNames;
			LocalDisplayNames.Add(TEXT("CForgeVehicle"));
			FDebugDisplayInfo LocalDisplayInfo(LocalDisplayNames, TArray<FName>());

			Vehicle->DisplayDebug(Canvas, LocalDisplayInfo, YL, YPos);
		}
	}
}

void AForgeBaseVehicle::DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	if (DebugDisplay.IsDisplayOn(TEXT("CForgeVehicle")))
	{
		FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
		TArray<FString> ClientStrings;
		GenerateDebugStrings(ClientStrings);

		DisplayDebugManager.DrawString(TEXT("---CLIENT VEHICLE STATE---"));
		for (const FString& String : ClientStrings)
		{
			DisplayDebugManager.DrawString(String, 15);
		}

		DisplayDebugManager.DrawString(TEXT("---SERVER VEHICLE STATE---"));
		for (const FString& String : ServerDebugStrings)
		{
			DisplayDebugManager.DrawString(String, 15);
		}

		//Ask for the server strings.  This is very chatty, but it's debug.
		if (GetLocalRole() != ROLE_Authority && ShouldRequestDebugStrings())
		{
			ServerPrintDebug_Request();
		}

	}
}

void AForgeBaseVehicle::GenerateDebugStrings(TArray<FString>& OutStrings)
{
	TArray<UForgeVehicleSeatConfig*> AllSeats;
	GetAllSeats(AllSeats);

	for (int i = 0; i < AllSeats.Num(); i++)
	{
		UForgeVehicleSeatConfig* SeatConfig = AllSeats[i];

		OutStrings.Add(FString::Printf(TEXT("\t[%d]: %s"), i, *ForgeVehicleBaseDebug::PrintDebugSeatInfo(SeatConfig)));
		if (IsValid(SeatConfig->PlayerSeatComponent))
		{
			OutStrings.Add(FString::Printf(TEXT("\t\tPlayer Last SeatChange Event: %s"), *UEnum::GetValueAsString(SeatConfig->PlayerSeatComponent->DebugLastSeatChangeType)));
		}
	}
}

void AForgeBaseVehicle::ServerPrintDebug_Request_Implementation()
{
	ServerDebugStrings.Empty(ServerDebugStrings.Num());
	GenerateDebugStrings(ServerDebugStrings);
}

bool AForgeBaseVehicle::ServerPrintDebug_Request_Validate()
{
	return true;
}

bool AForgeBaseVehicle::ShouldRequestDebugStrings() const
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

void AForgeBaseVehicle::OnRep_ServerDebugStrings()
{

}

