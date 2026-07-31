// Copyright Impact-Forge. Jammer emitter and registry implementation.

#include "Systems/ForgeDroneJammerComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Math/ForgeDroneMath.h"
#include "Net/UnrealNetwork.h"

// ---------------------------------------------------------------- emitter

UForgeDroneJammerComponent::UForgeDroneJammerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UForgeDroneJammerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeDroneJammerComponent, bActive);
}

void UForgeDroneJammerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Registered on every machine: the server evaluates denial, clients use the registry to drive
	// emitter FX and "you are being jammed" warnings.
	if (UForgeDroneJammerSubsystem* Registry = UForgeDroneJammerSubsystem::Get(this))
	{
		Registry->RegisterJammer(this);
	}
}

void UForgeDroneJammerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UForgeDroneJammerSubsystem* Registry = UForgeDroneJammerSubsystem::Get(this))
	{
		Registry->UnregisterJammer(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UForgeDroneJammerComponent::SetJammerActive(const bool bNewActive)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bActive == bNewActive)
	{
		return;
	}
	bActive = bNewActive;
	OnRep_Active();
}

void UForgeDroneJammerComponent::OnRep_Active()
{
	// Presentation is game-side; the component only owns the state. Blueprints hook the component's
	// activation through their own logic or by polling IsJammerActive.
}

float UForgeDroneJammerComponent::ComputeDenialAt(const FVector& WorldPosition, const FGameplayTag& LinkBand) const
{
	if (!bActive || !GetOwner())
	{
		return 0.f;
	}

	// An emitter with no band set is broadband. A banded emitter only touches links on its band, so a
	// faction can field a jammer their own drones fly straight through.
	if (BandTag.IsValid() && LinkBand.IsValid() && !LinkBand.MatchesTag(BandTag))
	{
		return 0.f;
	}

	const float DistanceM = FVector::Dist(GetOwner()->GetActorLocation(), WorldPosition) / 100.f;
	return ForgeDrone::Link::JamContribution(DistanceM, RadiusM, Power, FalloffExponent);
}

// ---------------------------------------------------------------- registry

UForgeDroneJammerSubsystem* UForgeDroneJammerSubsystem::Get(const UObject* WorldContextObject)
{
	if (WorldContextObject)
	{
		if (const UWorld* World = WorldContextObject->GetWorld())
		{
			return World->GetSubsystem<UForgeDroneJammerSubsystem>();
		}
	}
	return nullptr;
}

void UForgeDroneJammerSubsystem::RegisterJammer(UForgeDroneJammerComponent* Jammer)
{
	if (Jammer)
	{
		Jammers.AddUnique(Jammer);
	}
}

void UForgeDroneJammerSubsystem::UnregisterJammer(UForgeDroneJammerComponent* Jammer)
{
	Jammers.Remove(Jammer);
}

int32 UForgeDroneJammerSubsystem::GetActiveJammerCount() const
{
	int32 Count = 0;
	for (const UForgeDroneJammerComponent* Jammer : Jammers)
	{
		if (IsValid(Jammer) && Jammer->IsJammerActive())
		{
			++Count;
		}
	}
	return Count;
}

float UForgeDroneJammerSubsystem::ComputeJamFactor(const FVector& DronePosition, const FVector& OperatorPosition, const FGameplayTag LinkBand) const
{
	float Total = 0.f;

	for (const UForgeDroneJammerComponent* Jammer : Jammers)
	{
		if (!IsValid(Jammer))
		{
			continue;
		}

		// Evaluate against whichever end of the link this emitter sits closer to. Jamming attacks the
		// weaker end, so a jammer parked beside the enemy operator is as effective as one over the
		// target - and both are worth doing.
		const float DroneDenial = Jammer->ComputeDenialAt(DronePosition, LinkBand);
		const float OperatorDenial = Jammer->ComputeDenialAt(OperatorPosition, LinkBand);
		Total += FMath::Max(DroneDenial, OperatorDenial);

		if (Total >= 1.f)
		{
			return 1.f;
		}
	}

	return FMath::Clamp(Total, 0.f, 1.f);
}
