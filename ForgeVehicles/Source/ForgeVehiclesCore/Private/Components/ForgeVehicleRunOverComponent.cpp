// Copyright Impact-Forge.

#include "Components/ForgeVehicleRunOverComponent.h"

#include "ForgeVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UForgeVehicleRunOverComponent::UForgeVehicleRunOverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

	RunOverActorFilter = APawn::StaticClass();
	RunOverDamageType = UDamageType::StaticClass();
}

void UForgeVehicleRunOverComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		RegisterPrimitive(Cast<UPrimitiveComponent>(Owner->GetRootComponent()));
	}
}

void UForgeVehicleRunOverComponent::AddCollisionComponent(UPrimitiveComponent* Component)
{
	RegisterPrimitive(Component);
}

void UForgeVehicleRunOverComponent::RegisterPrimitive(UPrimitiveComponent* Primitive)
{
	if (!Primitive)
	{
		return;
	}

	Primitive->SetNotifyRigidBodyCollision(true);
	Primitive->OnComponentHit.AddUniqueDynamic(this, &UForgeVehicleRunOverComponent::OnComponentHit);
	Primitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UForgeVehicleRunOverComponent::OnComponentBeginOverlap);
}

void UForgeVehicleRunOverComponent::OnComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	HandlePotentialRunOver(OtherActor, Hit);
}

void UForgeVehicleRunOverComponent::OnComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	HandlePotentialRunOver(OtherActor, SweepResult);
}

bool UForgeVehicleRunOverComponent::CanRunOver(const AActor* OtherActor) const
{
	if (!OtherActor || OtherActor == GetOwner())
	{
		return false;
	}

	// Never "run over" another vehicle (or our own seat pawns).
	if (OtherActor->IsA(AForgeVehiclePawn::StaticClass()))
	{
		return false;
	}

	const UClass* Filter = RunOverActorFilter ? RunOverActorFilter.Get() : APawn::StaticClass();
	return OtherActor->IsA(Filter);
}

void UForgeVehicleRunOverComponent::HandlePotentialRunOver(AActor* OtherActor, const FHitResult& Hit)
{
	AActor* Owner = GetOwner();
	if (!Owner || !CanRunOver(OtherActor))
	{
		return;
	}

	const float Speed = Owner->GetVelocity().Size();
	if (Speed < MinRunOverSpeed)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// Per-actor cooldown so a single pass doesn't apply damage every physics sub-step.
	if (const float* LastTime = RecentlyRunOver.Find(OtherActor))
	{
		if (Now - *LastTime < PerActorCooldown)
		{
			return;
		}
	}
	RecentlyRunOver.Add(OtherActor, Now);

	// Damage and authoritative gameplay only run on the server.
	if (!Owner->HasAuthority())
	{
		OnRunOverActor.Broadcast(OtherActor, Speed, Hit);
		return;
	}

	float Damage = BaseRunOverDamage;
	if (bScaleDamageWithSpeed)
	{
		const float Alpha = FMath::Clamp((Speed - MinRunOverSpeed) / FMath::Max(1.f, FullDamageSpeed - MinRunOverSpeed), 0.f, 1.f);
		Damage = FMath::Lerp(BaseRunOverDamage, MaxRunOverDamage, Alpha);
	}

	AController* InstigatorController = Owner->GetInstigatorController();
	const FVector HitDirection = Owner->GetVelocity().GetSafeNormal();

	UGameplayStatics::ApplyPointDamage(OtherActor, Damage, HitDirection, Hit, InstigatorController, Owner, RunOverDamageType);

	OnRunOverActor.Broadcast(OtherActor, Speed, Hit);
}
