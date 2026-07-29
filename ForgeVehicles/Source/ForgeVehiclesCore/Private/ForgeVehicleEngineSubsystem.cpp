// Copyright Impact-Forge. Based on ArcVehicles by Puny Human. Ported to Chaos-only (UE5).

#include "ForgeVehicleEngineSubsystem.h"

#include "PhysicsEngine/BodyInstance.h"
#include "Physics/PhysicsInterfaceCore.h"

// UE5 is Chaos-only; the legacy PhysX simulation-filter-shader path from the original ArcVehicles
// plugin has been removed. Per-pair collision suppression is now handled through the Chaos
// "disabled collisions" API, matching what the seat attachment system expects.

void UForgeVehicleEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

static FPhysicsActorHandle GetActorHandle(UPrimitiveComponent* Component)
{
	if (Component && Component->GetBodyInstance())
	{
		return Component->GetBodyInstance()->GetPhysicsActorHandle();
	}
	return FPhysicsActorHandle();
}

bool UForgeVehicleEngineSubsystem::IgnoreBetween(UPrimitiveComponent* ObjA, UPrimitiveComponent* ObjB)
{
	if (ObjA == ObjB || ObjA == nullptr || ObjB == nullptr)
	{
		return false;
	}
	if (HasIgnoreBetween(ObjA, ObjB))
	{
		return true;
	}

	FIgnorePair IgnorePair;
	IgnorePair.ObjA = ObjA;
	IgnorePair.ObjB = ObjB;

	FPhysicsActorHandle ObjAHandle = GetActorHandle(ObjA);
	FPhysicsActorHandle ObjBHandle = GetActorHandle(ObjB);

	if (ObjAHandle != nullptr && ObjBHandle != nullptr)
	{
		FPhysicsInterface::ExecuteWrite(ObjAHandle, ObjBHandle, [](const FPhysicsActorHandle& A, const FPhysicsActorHandle& B)
		{
			TMap<FPhysicsActorHandle, TArray<FPhysicsActorHandle>> Map;
			Map.Add(A, { B });

			FPhysicsInterface::AddDisabledCollisionsFor_AssumesLocked(Map);
		});
	}

	return IgnoreComponents.Add(IgnorePair) >= 0;
}

bool UForgeVehicleEngineSubsystem::RemoveIgnoreBetween(UPrimitiveComponent* ObjA, UPrimitiveComponent* ObjB)
{
	int32 Removals = 0;
	for (int32 i = 0; i < IgnoreComponents.Num(); i++)
	{
		FIgnorePair& IP = IgnoreComponents[i];

		if ((IP.ObjA == ObjA || IP.ObjA == ObjB)
			&& (IP.ObjB == ObjA || IP.ObjB == ObjB))
		{
			IgnoreComponents.RemoveAt(i);
			i--;
			Removals++;
		}
	}

	FPhysicsActorHandle ObjAHandle = GetActorHandle(ObjA);
	FPhysicsActorHandle ObjBHandle = GetActorHandle(ObjB);

	if (ObjAHandle != nullptr && ObjBHandle != nullptr)
	{
		FPhysicsInterface::ExecuteWrite(ObjAHandle, ObjBHandle, [](const FPhysicsActorHandle& A, const FPhysicsActorHandle& B)
		{
			TArray<FPhysicsActorHandle> Actors{ A, B };
			FPhysicsInterface::RemoveDisabledCollisionsFor_AssumesLocked(Actors);
		});
	}

	return Removals > 0;
}

bool UForgeVehicleEngineSubsystem::HasIgnoreBetween(UPrimitiveComponent* ObjA, UPrimitiveComponent* ObjB)
{
	for (int32 i = 0; i < IgnoreComponents.Num(); i++)
	{
		FIgnorePair& IP = IgnoreComponents[i];

		if ((IP.ObjA == ObjA || IP.ObjA == ObjB)
			&& (IP.ObjB == ObjA || IP.ObjB == ObjB))
		{
			return true;
		}
	}

	return false;
}
