// Copyright Impact-Forge. Water-craft physics authored for Forge Vehicles.

#include "ForgeBuoyancyComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"

UForgeBuoyancyComponent::UForgeBuoyancyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// A sensible default hull footprint (four corners + centre) so the component floats something
	// out of the box; designers override these to match their hull mesh.
	Pontoons = {
		FVector(150.f, 90.f, -20.f),
		FVector(150.f, -90.f, -20.f),
		FVector(-150.f, 90.f, -20.f),
		FVector(-150.f, -90.f, -20.f),
		FVector(0.f, 0.f, -20.f)
	};
}

void UForgeBuoyancyComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveTargetPrimitive();

	if (TargetPrimitive && TargetPrimitive->IsSimulatingPhysics())
	{
		TargetPrimitive->SetLinearDamping(WaterLinearDamping);
		TargetPrimitive->SetAngularDamping(WaterAngularDamping);
	}
}

void UForgeBuoyancyComponent::ResolveTargetPrimitive()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (TargetComponentName != NAME_None)
	{
		TArray<UPrimitiveComponent*> Primitives;
		Owner->GetComponents<UPrimitiveComponent>(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive && Primitive->GetFName() == TargetComponentName)
			{
				TargetPrimitive = Primitive;
				return;
			}
		}
	}

	TargetPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
}

void UForgeBuoyancyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!TargetPrimitive || !TargetPrimitive->IsSimulatingPhysics() || Pontoons.Num() == 0)
	{
		return;
	}

	const FTransform BodyTransform = TargetPrimitive->GetComponentTransform();

	float SubmergedAccum = 0.f;
	for (const FVector& LocalPontoon : Pontoons)
	{
		const FVector WorldPontoon = BodyTransform.TransformPosition(LocalPontoon);
		const float Depth = WaterHeight - WorldPontoon.Z;
		const float Submersion = FMath::Clamp(Depth / PontoonRadius, 0.f, 1.f);
		SubmergedAccum += Submersion;

		if (Submersion > 0.f)
		{
			const FVector Force = FVector(0.f, 0.f, BuoyancyForcePerPontoon * Submersion);
			TargetPrimitive->AddForceAtLocation(Force, WorldPontoon);

			if (bShowDebug)
			{
				DrawDebugPoint(GetWorld(), WorldPontoon, 12.f, FColor::Cyan, false, -1.f);
				DrawDebugLine(GetWorld(), WorldPontoon, WorldPontoon + FVector(0.f, 0.f, Submersion * 100.f), FColor::Blue, false, -1.f);
			}
		}
	}

	LastSubmersionRatio = SubmergedAccum / static_cast<float>(Pontoons.Num());
}
