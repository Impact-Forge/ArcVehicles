// Copyright Impact-Forge. Drone autopilot: station keeping, loiter, return to home, terminal dive.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Math/ForgePID.h"

#include "ForgeDroneAutopilotComponent.generated.h"

/** What the autopilot is currently doing. */
UENUM(BlueprintType)
enum class EForgeDroneAutopilotMode : uint8
{
	/* Hands off - the pilot flies. */
	Manual,
	/* Hold current altitude, leaving translation to the pilot. */
	AltitudeHold,
	/* Hold a fixed point in space. Multirotor station keeping. */
	PositionHold,
	/* Fly a circle around a point. The standard observation pattern. */
	Orbit,
	/* Navigate back to the launch point. */
	ReturnToHome,
	/* Run in on a target at full throttle. Loitering munitions and kamikaze quads. */
	TerminalDive
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeAutopilotModeChanged, EForgeDroneAutopilotMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnForgeAutopilotArrived);

/**
 * Flies the drone when the pilot is not.
 *
 * Everything is issued through IForgeVehicleMovementInterface rather than by touching physics, so the
 * same component flies a multirotor and a fixed-wing without knowing which it has: it only needs to
 * know whether translation comes from tilting (multirotor) or from banking and pitching (fixed-wing).
 *
 * Beyond pilot convenience this is what makes the link model meaningful - a drone that loses its
 * signal has to do *something*, and hovering, coming home or pressing on are all tactically different
 * outcomes. Server-authoritative: the aircraft is flown where its physics is simulated.
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESDRONES_API UForgeDroneAutopilotComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeDroneAutopilotComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// End UActorComponent interface

	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Autopilot")
	void SetMode(EForgeDroneAutopilotMode NewMode);

	/**
	 * Engage a mode as a link-loss failsafe. Tracked separately from a pilot-commanded mode so that
	 * regaining signal returns control, without cancelling an orbit or dive the pilot asked for.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Autopilot")
	void SetModeFromFailsafe(EForgeDroneAutopilotMode NewMode);

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Autopilot")
	EForgeDroneAutopilotMode GetMode() const { return Mode; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Autopilot")
	bool WasEngagedByFailsafe() const { return bEngagedByFailsafe; }

	/* Circle to fly: centre, radius in metres, ground speed in m/s. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Autopilot")
	void SetOrbit(const FVector& InCentre, float InRadiusM, float InSpeedMS, bool bInClockwise = true);

	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Autopilot")
	void SetHomeLocation(const FVector& InHome) { HomeLocation = InHome; }

	/* Fixed point to run in on. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Autopilot")
	void SetDiveTarget(const FVector& InTarget);

	/* Moving target to run in on; tracked while it lives. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Autopilot")
	void SetDiveTargetActor(AActor* InTargetActor);

	/* Point currently being held or flown to. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Autopilot")
	FVector GetTargetLocation() const { return TargetLocation; }

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Autopilot")
	FOnForgeAutopilotModeChanged OnModeChanged;

	/* Fired when a ReturnToHome or navigation leg reaches its destination. */
	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Autopilot")
	FOnForgeAutopilotArrived OnArrived;

protected:

	/* Cruise speed used for navigation legs, m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot", meta = (ClampMin = "0.1"))
	float CruiseSpeedMS = 12.f;

	/* Altitude above the home point used when returning, metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot", meta = (ClampMin = "0.0"))
	float ReturnAltitudeM = 60.f;

	/* How close counts as arrived, metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot", meta = (ClampMin = "0.5"))
	float ArrivalToleranceM = 8.f;

	/* Largest lean the autopilot will command from a multirotor, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float MaxAutoTiltDeg = 25.f;

	/* Largest bank the autopilot will command from a fixed-wing, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float MaxAutoBankDeg = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot|Tuning")
	FForgePIDController AltitudePID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot|Tuning")
	FForgePIDController ForwardPID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot|Tuning")
	FForgePIDController LateralPID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot|Tuning")
	FForgePIDController HeadingPID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Autopilot|Tuning")
	FForgePIDController SpeedPID;

	/* Recomputes TargetLocation for the active mode. */
	void UpdateTarget(float DeltaTime);

	/* Multirotor: translate by tilting toward the target, hold altitude on the collective. */
	void FlyMultirotor(float DeltaTime);

	/* Fixed-wing: bank toward the target, pitch for altitude, throttle for speed. */
	void FlyFixedWing(float DeltaTime);

	/* Zeroes every axis, so releasing the autopilot does not leave a stale command applied. */
	void ReleaseControls();

	void ResetLoops();

	EForgeDroneAutopilotMode Mode = EForgeDroneAutopilotMode::Manual;
	bool bEngagedByFailsafe = false;

	/* True when translation comes from tilting rather than banking. */
	bool bMultirotorProfile = false;

	FVector TargetLocation = FVector::ZeroVector;
	FVector HomeLocation = FVector::ZeroVector;

	FVector OrbitCentre = FVector::ZeroVector;
	float OrbitRadiusM = 80.f;
	float OrbitSpeedMS = 14.f;
	bool bOrbitClockwise = true;
	/* Angle around the orbit, radians. Advanced by speed so the drone flies the circle. */
	float OrbitPhase = 0.f;

	UPROPERTY()
	TWeakObjectPtr<AActor> DiveTargetActor;

	bool bArrivalReported = false;
};
