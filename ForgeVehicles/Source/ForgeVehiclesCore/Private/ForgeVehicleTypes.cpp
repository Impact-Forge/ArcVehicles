// Copyright 2017-2020 Puny Human, All Rights Reserved.

#include "ForgeVehiclesCore.h"
#include "ForgeVehicleTypes.h"
#include "ForgeBaseVehicle.h"
#include "ForgeVehicleSeatConfig.h"

class USceneComponent* FForgeOwnerAttachmentReference::GetSceneComponent(AActor* OwnerActor)
{
	TInlineComponentArray<USceneComponent*> Components;
	OwnerActor->GetComponents(Components);

	for (USceneComponent* Comp : Components)
	{
		if (Comp->GetFName() == ComponentName)
		{
			return Comp;
		}
	}

	return nullptr;
}

FForgeVehicleScopedRelativeTransformRestoration::FForgeVehicleScopedRelativeTransformRestoration(AActor* InActor)
{
	if (IsValid(InActor))
	{
		TInlineComponentArray<USceneComponent*> Components(InActor);
		for (USceneComponent* Comp : Components)
		{
			if (Comp == InActor->GetRootComponent())
			{
				continue;
			}

			ComponentTransformMap.Add(Comp, Comp->GetRelativeTransform());
		}
	}
}

FForgeVehicleScopedRelativeTransformRestoration::FForgeVehicleScopedRelativeTransformRestoration()
{

}

FForgeVehicleScopedRelativeTransformRestoration::~FForgeVehicleScopedRelativeTransformRestoration()
{
	Restore();
}

void FForgeVehicleScopedRelativeTransformRestoration::Restore()
{
	for (const auto& KVP : ComponentTransformMap)
	{
		USceneComponent* Comp = KVP.Key.Get();
		const FTransform& Transform = KVP.Value;

		if (IsValid(Comp))
		{
			Comp->SetRelativeTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

FForgeVehicleSeatReference::FForgeVehicleSeatReference(UForgeVehicleSeatConfig* SeatConfig)
	: Vehicle(nullptr)
	, SeatID(INDEX_NONE)
{
	if (::IsValid(SeatConfig))
	{
		Vehicle = SeatConfig->GetVehicleOwner();
		if (::IsValid(Vehicle))
		{
			SeatID = Vehicle->GetSeatIndex(SeatConfig);
		}
	}
}

UForgeVehicleSeatConfig* FForgeVehicleSeatReference::operator->()
{
	check(IsValid());
	UForgeVehicleSeatConfig* Seat = Vehicle->GetSeatConfig(*this);
	check(::IsValid(Seat));

	return Seat;
}

UForgeVehicleSeatConfig* FForgeVehicleSeatReference::operator*()
{
	if (!IsValid())
	{
		return nullptr;
	}
	return Vehicle->GetSeatConfig(*this);
}

bool FForgeVehicleSeatReference::IsValid() const
{
	return ::IsValid(Vehicle) && SeatID >= 0;
}

bool FForgeVehicleSeatReference::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar << Vehicle;
	Ar << SeatID;
	bOutSuccess = true;
	return true;
}

FString FForgeVehicleSeatReference::ToString() const
{
	return FString::Printf(TEXT("SeatRef %s(%d)"), ::IsValid(Vehicle) ? *Vehicle->GetName() : TEXT("null"), SeatID);
}
