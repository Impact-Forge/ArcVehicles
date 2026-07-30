// Copyright Impact-Forge. Control link: range, line of sight, jamming, and failsafe behaviour.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ForgeDroneLinkComponent.generated.h"

class AController;

/** What a drone does when it loses its control link. */
UENUM(BlueprintType)
enum class EForgeDroneLinkLossBehavior : uint8
{
	/* Keep flying on the last commands. Correct for a terminal dive already committed. */
	Continue,
	/* Hold position and altitude and wait for the link to come back. */
	FailsafeHover,
	/* Fly back to the launch point under autopilot. */
	ReturnToHome,
	/* Cut the motors immediately. Denies the wreck to the enemy. */
	CutMotors
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeLinkQualityChanged, float, LinkQuality);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnForgeLinkEvent);

/**
 * The radio link between a drone and whoever is flying it.
 *
 * Range, terrain and electronic warfare all attack the same thing: the pilot's ability to command the
 * aircraft. Rather than modelling those as separate gameplay rules, they fold into one quality value
 * that degrades control - inputs stutter and arrive late before they stop arriving at all - and then
 * hands the aircraft to its failsafe. That is what makes flying behind a ridge or past a jammer a
 * decision with a cost, instead of a hard boundary the pilot bounces off.
 *
 * Evaluated on the server a few times a second (a line trace per drone per frame would be wasteful),
 * with the resulting quality replicated for HUD and video-static effects.
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESDRONES_API UForgeDroneLinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeDroneLinkComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// End UActorComponent interface

	/* Maximum control range, metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link", meta = (ClampMin = "1.0"))
	float MaxRangeM = 5000.f;

	/* Fraction of MaxRangeM held at full quality before roll-off begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link", meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float FullQualityRangeFraction = 0.6f;

	/* Quality multiplier while terrain blocks the path to the operator. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OcclusionFactor = 0.35f;

	/* Frequency band, matched against jammer band tags. Empty means "affected by any emitter". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link")
	FGameplayTag BandTag;

	/* Quality below which inputs begin to stutter and lag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link|Degradation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DegradedQualityThreshold = 0.5f;

	/* Quality below which the link counts as lost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link|Degradation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LostQualityThreshold = 0.15f;

	/* Command latency at zero quality, seconds. Scales in as quality falls below the threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link|Degradation", meta = (ClampMin = "0.0"))
	float MaxInducedLatencySeconds = 0.25f;

	/* How long the link must stay lost before the failsafe engages. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link|Failsafe", meta = (ClampMin = "0.0"))
	float LinkLossGraceSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link|Failsafe")
	EForgeDroneLinkLossBehavior LinkLossBehavior = EForgeDroneLinkLossBehavior::FailsafeHover;

	/* Evaluations per second. Each one costs a single line trace. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float EvaluationHz = 4.f;

	/* How quickly quality follows its target. Stops thin obstructions flickering the signal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeDrone|Link", meta = (ClampMin = "0.1"))
	float QualitySmoothingPerSecond = 3.f;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Link")
	FOnForgeLinkQualityChanged OnLinkQualityChanged;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Link")
	FOnForgeLinkEvent OnLinkLost;

	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Link")
	FOnForgeLinkEvent OnLinkRegained;

	/**
	 * Bind the drone to whoever is flying it.
	 *
	 * @param InOperatorController Controller issuing commands.
	 * @param InOperatorAntenna    Actor the link is measured from - the operator's body, not their
	 *                             camera, since the radio is on the soldier while the view is on the
	 *                             drone.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Link")
	void SetOperator(AController* InOperatorController, AActor* InOperatorAntenna);

	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Link")
	void ClearOperator();

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Link")
	AController* GetOperatorController() const { return OperatorController.Get(); }

	/* Current link quality, 0-1. Replicated. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Link")
	float GetLinkQuality() const { return LinkQuality; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Link")
	bool IsLinkLost() const { return LinkQuality <= LostQualityThreshold; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Link")
	bool IsLinkDegraded() const { return LinkQuality < DegradedQualityThreshold; }

	/* Distance to the operator, metres. Negative when no operator is bound. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Link")
	float GetOperatorDistanceM() const;

	/* Launch point the ReturnToHome failsafe flies back to. Set on deploy. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Link")
	void SetHomeLocation(const FVector& InHomeLocation) { HomeLocation = InHomeLocation; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Link")
	FVector GetHomeLocation() const { return HomeLocation; }

	/**
	 * Single choke point every pilot command passes through.
	 *
	 * A healthy link returns the value untouched. A degraded one holds stale values for short windows
	 * and delays what does get through, so the aircraft feels like it is being flown down a bad radio
	 * rather than simply becoming less capable.
	 *
	 * @param AxisIndex Which control axis, so each is delayed independently.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Link")
	float FilterInput(int32 AxisIndex, float RawValue);

protected:

	UFUNCTION()
	void OnRep_LinkQuality();

	/* Recomputes the target quality from range, line of sight and jamming. Server only. */
	void EvaluateLink();

	/* Traces to the operator antenna. Returns 1 when clear, OcclusionFactor when blocked. */
	float EvaluateLineOfSight() const;

	/* Runs the configured failsafe once the grace period expires. */
	void EngageFailsafe();

	/* Restores manual control after the link comes back. */
	void ReleaseFailsafe();

	UPROPERTY(ReplicatedUsing = OnRep_LinkQuality)
	float LinkQuality = 1.f;

	UPROPERTY()
	TWeakObjectPtr<AController> OperatorController;

	UPROPERTY()
	TWeakObjectPtr<AActor> OperatorAntenna;

	FVector HomeLocation = FVector::ZeroVector;

	/* Smoothed line-of-sight factor, so a passing tree does not drop the link. */
	float LineOfSightFactor = 1.f;

	float TimeSinceEvaluation = 0.f;
	float TimeLinkLost = 0.f;
	bool bLinkLostReported = false;
	bool bFailsafeEngaged = false;

	/* Per-axis held values and remaining delay, for the degraded-input model. */
	static constexpr int32 NumFilteredAxes = 6;
	float HeldAxisValues[NumFilteredAxes] = { 0.f };
	float AxisHoldRemaining[NumFilteredAxes] = { 0.f };
};
