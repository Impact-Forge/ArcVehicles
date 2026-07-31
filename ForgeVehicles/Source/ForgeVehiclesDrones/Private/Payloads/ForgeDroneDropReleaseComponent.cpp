// Copyright Impact-Forge. Store release implementation.

#include "Payloads/ForgeDroneDropReleaseComponent.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "ForgeVehiclesDrones.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UForgeDroneDropReleaseComponent::UForgeDroneDropReleaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UForgeDroneDropReleaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeDroneDropReleaseComponent, RemainingStores);
}

void UForgeDroneDropReleaseComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RemainingStores = StoreCapacity;
	}
}

bool UForgeDroneDropReleaseComponent::CanRelease() const
{
	if (RemainingStores <= 0 || !StoreClass || !GetWorld())
	{
		return false;
	}
	return GetWorld()->GetTimeSeconds() - LastReleaseTime >= ReleaseIntervalSeconds;
}

void UForgeDroneDropReleaseComponent::Rearm()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RemainingStores = StoreCapacity;
	}
}

AController* UForgeDroneDropReleaseComponent::ResolveInstigatorController() const
{
	// Prefer whoever is flying the drone right now.
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AController* Controller = OwnerPawn->GetController())
		{
			return Controller;
		}
	}
	// Otherwise fall back to whoever deployed it, so an autonomous drop still credits someone.
	if (const AActor* Owner = GetOwner())
	{
		if (const APawn* InstigatorPawn = Owner->GetInstigator())
		{
			return InstigatorPawn->GetController();
		}
	}
	return nullptr;
}

AActor* UForgeDroneDropReleaseComponent::ReleaseStore()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !CanRelease())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Release from the authored socket when there is one, so stores leave from the rack rather than
	// the middle of the airframe.
	FVector ReleaseLocation = Owner->GetActorLocation();
	if (!ReleaseSocket.IsNone())
	{
		TInlineComponentArray<UMeshComponent*> Meshes(Owner);
		for (const UMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->DoesSocketExist(ReleaseSocket))
			{
				ReleaseLocation = Mesh->GetSocketLocation(ReleaseSocket);
				break;
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Cast<APawn>(Owner);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Store = World->SpawnActor<AActor>(StoreClass, ReleaseLocation, Owner->GetActorRotation(), SpawnParams);
	if (!Store)
	{
		return nullptr;
	}

	// Inherit the drone's velocity plus a small separation push. This is why dropping accurately is
	// about judging the run-in: the store carries the aircraft's motion with it.
	const FVector InheritedVelocity = Owner->GetVelocity();
	const FVector ReleaseVelocity = InheritedVelocity - FVector(0.f, 0.f, SeparationImpulseCmS);

	if (UPrimitiveComponent* StoreRoot = Cast<UPrimitiveComponent>(Store->GetRootComponent()))
	{
		if (StoreRoot->IsSimulatingPhysics())
		{
			StoreRoot->SetPhysicsLinearVelocity(ReleaseVelocity);
		}
	}
	Store->SetInstigator(SpawnParams.Instigator);

	--RemainingStores;
	LastReleaseTime = World->GetTimeSeconds();

	OnStoreReleased.Broadcast(Store, RemainingStores);
	UE_LOG(LogForgeDrones, Verbose, TEXT("%s released a store (%d remaining)."), *GetNameSafe(Owner), RemainingStores);

	return Store;
}
