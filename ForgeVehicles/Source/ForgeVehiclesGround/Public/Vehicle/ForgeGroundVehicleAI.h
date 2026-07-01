//Copyright 2025 P.Kallisto 

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h" 
#include "Components/ActorComponent.h"
#include "ForgeGroundVehicleAI.generated.h"


UCLASS(ClassGroup = ("R-Tune"), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESGROUND_API UForgeGroundVehicleAI : public UActorComponent
{
	GENERATED_BODY()

public:	

	UForgeGroundVehicleAI();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "R-Tune AI")
    void Update(float deltaTime, float& OutSteering);

    UFUNCTION(BlueprintCallable, Category = "R-Tune AI")
    void SetTargetSpline(USplineComponent* InSplineComponent);

    //Resets the internal state. Useful when re-engaging spline following or changing splines.
    UFUNCTION(BlueprintCallable, Category = "R-Tune AI")
    void Refresh();


protected:

	virtual void BeginPlay() override;

private:

    //Proportional gain coefficient. This controls how quickly the system responds. Setting this too high can cause instability and overshooting.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID Settings", meta = (ClampMin = "0.0"), meta = (AllowPrivateAccess="true"))
    float Kp = 0.3f;

    //Derivative coefficient. This is useful for damping the movement and increase smoothness 
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID Settings", meta = (ClampMin = "0.0"), meta = (AllowPrivateAccess = "true"))
    float Kd = 0.2f;

    //Second derivative coefficient. This can be used to damp the system as a whole. This is more useful than 'Kd' for smoothing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID Settings", meta = (AllowPrivateAccess = "true"))
    float Kd2 = 0.2f;

    //Integral gain coefficient. This is useful in helping correct steady state offsets.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID Settings", meta = (ClampMin = "0.0"), meta = (AllowPrivateAccess = "true"))
    float Ki = 0.5f;

    //Maximum buildup for integral accumalation (Ki)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID Settings", meta = (ClampMin = "0.0"), meta = (AllowPrivateAccess = "true"))
    float MaxIntegral = 20.0f;

    //Maximum steering output. This is effectively a clamp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID Settings", meta = (AllowPrivateAccess = "true"))
    float OutputMax = 1.0f;

    //This is the minimum delta time that integral and derivate computations will take place.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID Settings", meta = (ClampMin = "0.0"), meta = (AllowPrivateAccess = "true"))
    float MinDeltaTime = 0.0001f;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID Settings", meta = (ClampMin = "0.0"), meta = (AllowPrivateAccess = "true"))
     bool bDrawDebug = false;

    USplineComponent* TargetSpline;

    AActor* OwnerActor;
    UWorld* World;
    bool bIsFirstUpdate = true;

    float BounceVelocity = 0.f;
    float LastDelta = 0.f;
    float LateralError = 0.f;

    float IntegralError = 0.0f;
    float PreviousError = 0.0f;

    float CalculatePIDOutput(float Error, float DeltaTime);
		
};
