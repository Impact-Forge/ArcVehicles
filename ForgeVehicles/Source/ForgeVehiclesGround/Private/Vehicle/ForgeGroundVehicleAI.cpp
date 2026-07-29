//Copyright 2025 P.Kallisto 


#include "Vehicle/ForgeGroundVehicleAI.h"
#include "Kismet/KismetMathLibrary.h"  
#include "DrawDebugHelpers.h"    


UForgeGroundVehicleAI::UForgeGroundVehicleAI()
{
	PrimaryComponentTick.bCanEverTick = true;

}


void UForgeGroundVehicleAI::BeginPlay()
{
	Super::BeginPlay();

    OwnerActor = GetOwner();
    World = GetWorld();
	
}


void UForgeGroundVehicleAI::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UForgeGroundVehicleAI::SetTargetSpline(USplineComponent* InSplineComponent)
{
    if (InSplineComponent != nullptr)
    {
        TargetSpline = InSplineComponent;
        Refresh();
    }
}

void UForgeGroundVehicleAI::Update(float deltaTime, float& OutSteering)
{
    if (OwnerActor == nullptr)
    {
        return;
    }

    FVector location = OwnerActor->GetActorLocation();

    float key = TargetSpline->FindInputKeyClosestToWorldLocation(location);

    FVector approxPoint = TargetSpline->GetLocationAtSplineInputKey(key, ESplineCoordinateSpace::World);
    FVector direction = TargetSpline->GetDirectionAtSplineInputKey(key, ESplineCoordinateSpace::World).GetSafeNormal();
    FVector up = TargetSpline->GetUpVectorAtSplineInputKey(key, ESplineCoordinateSpace::World).GetSafeNormal();

    const FVector directionVector = location - approxPoint;
    FVector splineVector = FVector::CrossProduct(direction, up);
    splineVector.Normalize();

    LastDelta = LateralError;
    LateralError = FVector::DotProduct(directionVector, splineVector);

    BounceVelocity = (LateralError - LastDelta) * (1.f / deltaTime);
    float DampingForce = -BounceVelocity * Kd2;

    OutSteering = CalculatePIDOutput(LateralError - DampingForce, deltaTime);

    if (bDrawDebug)
    {
        DrawDebugSphere(World, location, 20.0f, 12, FColor::Blue, false, 0.0f, 1, 1.0f);
        DrawDebugSphere(World, approxPoint, 20.0f, 12, FColor::Red, false, 0.0f, 1, 1.0f);
        DrawDebugLine(World, location, approxPoint, FColor::Green, false, 0.0f, 1, 2.0f);
    }
}

void UForgeGroundVehicleAI::Refresh()
{
	IntegralError = 0.0f;
	PreviousError = 0.0f;
	bIsFirstUpdate = true;
}

float UForgeGroundVehicleAI::CalculatePIDOutput(float Error, float DeltaTime)
{

    if (DeltaTime <= MinDeltaTime)
    {
        if (!FMath::IsNearlyZero(Kd) && !bIsFirstUpdate)
        {
            return FMath::Clamp(Kp * Error, -OutputMax, OutputMax);
        }

        return FMath::Clamp(Kp * Error, -OutputMax, OutputMax);
    }

    float P_Term = Kp * Error;

    // Integral Term
    IntegralError += Error * DeltaTime;
    IntegralError = FMath::Clamp(IntegralError, -MaxIntegral, MaxIntegral);
    float I_Term = Ki * IntegralError;


    float D_Term = 0.0f;
    if (!bIsFirstUpdate)
    {
        float DerivativeError = (Error - PreviousError) / DeltaTime;
        D_Term = Kd * DerivativeError;
    }

    PreviousError = Error;
    bIsFirstUpdate = false;

    float Output = P_Term + I_Term + D_Term;
    return FMath::Clamp(Output, -OutputMax, OutputMax);
}

