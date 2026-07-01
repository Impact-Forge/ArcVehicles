//Copyright 2025 P.Kallisto 



#include "Vehicle/StaticVehicleWheel.h"
#include "Kismet/KismetMathLibrary.h"


UStaticVehicleWheel::UStaticVehicleWheel()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStaticVehicleWheel::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(true);

	VehiclePrimitiveComponent = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
}

void UStaticVehicleWheel::Update(float deltaTime)
{
	float scaling = 1.f / 100.f;
	FTransform t = VehiclePrimitiveComponent->GetComponentTransform();
	float speed = UKismetMathLibrary::InverseTransformDirection(t, VehiclePrimitiveComponent->GetComponentVelocity()).X * scaling;

	float v = GetComponentVelocity().Size() * scaling;
	GetLocalBounds(minBound, maxBound);
	float rotation = v / (maxBound.Z * scaling);

	float localRotation = bInvertDirection ? -1.f : 1.f;
	float d = speed < 0.f ? 1.f : -1.f;

	float finalRotation = rotation * localRotation * d * RotationMultiplier * (bEnabled ? 1.f : 0.f);
	AddLocalRotation(FRotator(finalRotation, 0.f, 0.f));

}


void UStaticVehicleWheel::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Update(DeltaTime);
}