// Copyright Impact-Forge. Two-axis stabilised camera gimbal.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "ForgeDroneGimbalComponent.generated.h"

class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeGimbalAimChanged, FRotator, AimRotation);

/**
 * Two-axis camera gimbal that holds its aim in world space while the airframe manoeuvres.
 *
 * Stabilisation is the whole point: an unstabilised camera bolted to a drone is unusable for
 * observation, because every gust and correction throws the picture around. The gimbal is commanded in
 * world space and continuously compensates for the airframe's attitude, so the operator aims at
 * ground features rather than fighting the aircraft.
 *
 * Deliberately not built on the existing turret movement component: that one is shaped around a gunner
 * seat pawn driving its own controller view, which a seatless drone does not have.
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESDRONES_API UForgeDroneGimbalComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeDroneGimbalComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// End UActorComponent interface

	/* Component rotated in yaw. Usually the gimbal ring the camera hangs from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Gimbal")
	FName YawComponentName = NAME_None;

	/* Component rotated in pitch. Usually the camera cradle, parented under the yaw ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Gimbal")
	FName PitchComponentName = NAME_None;

	/* Pitch travel limits, degrees. Straight down is -90. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Gimbal")
	float MinPitchDeg = -90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Gimbal")
	float MaxPitchDeg = 30.f;

	/* Yaw travel limit either side of forward, degrees. 180 allows continuous rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Gimbal", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float YawLimitDeg = 180.f;

	/* Maximum slew rate, degrees/second. Real gimbals cannot snap instantly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Gimbal", meta = (ClampMin = "1.0"))
	float SlewRateDegPerSecond = 90.f;

	/* Operator look sensitivity, degrees per unit of input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Gimbal", meta = (ClampMin = "0.1"))
	float LookSensitivityDeg = 45.f;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Gimbal")
	FOnForgeGimbalAimChanged OnAimChanged;

	/* Operator look input, normalised. X yaws, Y pitches. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Gimbal")
	void AddAimInput(const FVector2D& LookInput, float DeltaTime);

	/* Point the camera at a world location. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Gimbal")
	void LookAt(const FVector& WorldLocation);

	/* Recentre on the airframe's forward axis. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Gimbal")
	void RecentreAim();

	/* Current world-space aim. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Gimbal")
	FRotator GetAimRotation() const { return DesiredWorldAim; }

	/* Where the camera is actually pointing, after slew limits. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Gimbal")
	FRotator GetCurrentAimRotation() const { return CurrentWorldAim; }

protected:

	UFUNCTION()
	void OnRep_ReplicatedAim();

	/* Pushes the world aim onto the yaw/pitch components, compensating for airframe attitude. */
	void ApplyAimToComponents();

	void ResolveComponents();

	/* Aim the operator has commanded, world space. */
	FRotator DesiredWorldAim = FRotator::ZeroRotator;

	/* Aim the gimbal has actually reached, world space, after slew limiting. */
	FRotator CurrentWorldAim = FRotator::ZeroRotator;

	/* Quantised world aim so observers see roughly where the camera points. */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedAim)
	FRotator ReplicatedAim = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> YawComponent;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PitchComponent;

	float TimeSinceAimReplication = 0.f;
};
