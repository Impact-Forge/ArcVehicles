// Copyright Impact-Forge. Warhead implementation.

#include "Payloads/ForgeDroneWarheadComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "ForgeVehiclesDrones.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UForgeDroneWarheadComponent::UForgeDroneWarheadComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Arming checks and the proximity fuze do not need frame accuracy.
	PrimaryComponentTick.TickInterval = 0.05f;
	SetIsReplicatedByDefault(true);
}

void UForgeDroneWarheadComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeDroneWarheadComponent, bArmed);
}

void UForgeDroneWarheadComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	LaunchLocation = Owner->GetActorLocation();

	// Contact fuzing rides the owner's own hit events, so it works with whatever collision the
	// airframe already has rather than needing a dedicated fuze primitive.
	Owner->OnActorHit.AddDynamic(this, &UForgeDroneWarheadComponent::HandleOwnerHit);

	if (bAutoArmOnDeploy)
	{
		RequestArm();
	}

	SetComponentTickEnabled(Owner->HasAuthority());
}

void UForgeDroneWarheadComponent::RequestArm()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bArmingRequested)
	{
		return;
	}

	bArmingRequested = true;
	TimeSinceArmRequest = 0.f;
	LaunchLocation = GetOwner()->GetActorLocation();
}

void UForgeDroneWarheadComponent::Safe()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bArmingRequested = false;
	TimeSinceArmRequest = 0.f;
	SetArmed(false);
}

bool UForgeDroneWarheadComponent::AreArmingConditionsMet() const
{
	if (!bArmingRequested || !GetOwner())
	{
		return false;
	}

	// Both conditions, deliberately: the delay stops a drone detonating in the operator's hands, and
	// the distance stops an armed drone being walked into a target as a hand grenade.
	if (TimeSinceArmRequest < ArmDelaySeconds)
	{
		return false;
	}

	const float DistanceM = FVector::Dist(GetOwner()->GetActorLocation(), LaunchLocation) / 100.f;
	return DistanceM >= MinArmDistanceM;
}

void UForgeDroneWarheadComponent::SetArmed(const bool bNewArmed)
{
	if (bArmed == bNewArmed)
	{
		return;
	}
	bArmed = bNewArmed;
	OnArmedChanged.Broadcast(bArmed);
}

void UForgeDroneWarheadComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bDetonated)
	{
		return;
	}

	if (bArmingRequested && !bArmed)
	{
		TimeSinceArmRequest += DeltaTime;
		if (AreArmingConditionsMet())
		{
			SetArmed(true);
		}
	}

	// Proximity fuze: for munitions meant to burst near a target rather than strike it.
	if (bArmed && ProximityFuzeRadiusM > 0.f && GetWorld())
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ForgeDroneProximityFuze), /*bTraceComplex*/ false);
		QueryParams.AddIgnoredActor(Owner);

		const bool bFound = GetWorld()->OverlapMultiByChannel(
			Overlaps,
			Owner->GetActorLocation(),
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeSphere(ProximityFuzeRadiusM * 100.f),
			QueryParams);

		if (bFound)
		{
			for (const FOverlapResult& Overlap : Overlaps)
			{
				AActor* Other = Overlap.GetActor();
				// Never fuze on the operator who launched it.
				if (!Other || Other == Owner->GetInstigator())
				{
					continue;
				}

				FHitResult ProximityHit;
				ProximityHit.ImpactPoint = Other->GetActorLocation();
				ProximityHit.Location = Owner->GetActorLocation();
				ProximityHit.ImpactNormal = (Owner->GetActorLocation() - Other->GetActorLocation()).GetSafeNormal();
				Detonate(ProximityHit);
				return;
			}
		}
	}
}

void UForgeDroneWarheadComponent::HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bDetonated || !bArmed)
	{
		return;
	}

	// A gentle bump - settling onto the ground, brushing a wall - should not set the warhead off; a
	// strike should. Speed is what separates them.
	const float ImpactSpeedMS = Owner->GetVelocity().Size() / 100.f;
	if (ImpactSpeedMS < MinImpactSpeedMS)
	{
		return;
	}

	Detonate(Hit);
}

void UForgeDroneWarheadComponent::Detonate(const FHitResult& Impact)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bDetonated)
	{
		return;
	}

	bDetonated = true;

	// The component decides *when*, never *what*. Whoever binds this owns the effect, which is what
	// keeps damage modelling out of the vehicle plugin.
	OnDetonated.Broadcast(Impact, this);

	UE_LOG(LogForgeDrones, Verbose, TEXT("%s warhead detonated."), *GetNameSafe(Owner));

	if (bDestroyOwnerOnDetonate)
	{
		Owner->Destroy();
	}
}

void UForgeDroneWarheadComponent::OnRep_Armed()
{
	// Clients drive arming indicators and audio from this.
	OnArmedChanged.Broadcast(bArmed);
}
