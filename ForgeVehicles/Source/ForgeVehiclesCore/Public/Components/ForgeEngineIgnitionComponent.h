// Copyright Impact-Forge. Ignition state machine generalised from the BurningLands reference implementation.

#pragma once

#include "Components/ActorComponent.h"
#include "ForgeEngineIgnitionComponent.generated.h"

/** The ignition state of a Forge vehicle engine. */
UENUM(BlueprintType)
enum class EForgeEngineIgnitionState : uint8
{
	Off			UMETA(ToolTip = "Engine is off and is not being started."),
	Igniting	UMETA(ToolTip = "Engine is being started up but has not yet completed the process."),
	Cutoff		UMETA(ToolTip = "Engine is being shut off."),
	On			UMETA(ToolTip = "Engine is on and started.")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FForgeEngineIgnitionStateChangedDelegate, class UForgeEngineIgnitionComponent*, IgnitionComponent, EForgeEngineIgnitionState, NewState, EForgeEngineIgnitionState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FForgeEngineReleasedIgnitionDelegate, class UForgeEngineIgnitionComponent*, IgnitionComponent, EForgeEngineIgnitionState, ReleaseState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FForgeEngineIgnitionCooldownElapsedDelegate, class UForgeEngineIgnitionComponent*, IgnitionComponent);

/**
 * Replicated engine ignition state machine shared by every Forge vehicle type (ground, rotary wing,
 * fixed wing and water craft). It owns the Off -> Igniting -> On / On -> Cutoff -> Off transitions,
 * the transient "release" behaviour and the post-change cooldown, broadcasting delegates that a
 * vehicle binds to in order to drive its movement solution.
 */
UCLASS(ClassGroup = (ForgeVehicles), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESCORE_API UForgeEngineIgnitionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeEngineIgnitionComponent();

	//~ Begin UActorComponent interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void InitializeComponent() override;
	// End UActorComponent interface

	/* Returns what state the Engine is in. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Engine Ignition")
	EForgeEngineIgnitionState GetEngineIgnitionState() const { return EngineIgnitionState; }

	/* Convenience: true only when the engine has fully started. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Engine Ignition")
	bool IsEngineRunning() const { return EngineIgnitionState == EForgeEngineIgnitionState::On; }

	/* Attempts to start up the Engine. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Engine Ignition")
	void RequestEngageIgnition();

	/* Attempts to shut down the Engine. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Engine Ignition")
	void RequestDisengageIgnition();

	/* Attempts to release the current Ignition state of the Engine. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Engine Ignition")
	void RequestReleaseIgnition();

	/* Returns the time left before ignition will be complete if engaging the ignition. -1.f otherwise. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Engine Ignition")
	float GetEngageIgnitionTimeRemaining() const;

	/* Returns the time left before ignition will be cutoff if disengaging the ignition. -1.f otherwise. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Engine Ignition")
	float GetDisengageIgnitionTimeRemaining() const;

	/* Returns the time left before the ignition cooldown will finish. -1.f otherwise. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Engine Ignition")
	float GetIgnitionCooldownTimeRemaining() const;

protected:

	/* Whether or not we can initiate a Startup of the Engine. */
	virtual bool CanEngageIgnition() const;

	/* Whether or not we can initiate a Shutdown of the Engine. */
	virtual bool CanDisengageIgnition() const;

	/* Whether or not we can release the current Ignition state of the Engine. */
	virtual bool CanReleaseIgnition() const;

	/* Initiates the Engine Ignition process. */
	virtual void EngageIgnition();

	/* Initiates the Engine Cutoff process. */
	virtual void DisengageIgnition();

	/* Releases the transient Ignition state of the Engine from either of the startup or shutdown processes. */
	virtual void ReleaseIgnition();

	/* Changes the EngineIgnitionState to the NewState. */
	virtual void ChangeState(EForgeEngineIgnitionState NewState);

	void StartIgnitionCooldown();

	virtual void IgnitionCooldownElapsed();

	virtual void EngagingIgnitionElapsed();

	virtual void DisengagingIgnitionElapsed();

	/**
	 * Tells the Server that we want to initiate Startup or Shutdown of the Engine.
	 *
	 * @param bDisengageIgnition	True if we are initiating a Shutdown instead of a Startup.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerInitiateIgnition(bool bDisengageIgnition = false);

	/* Tells the Server that we are releasing the Ignition state the Engine is in. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerReleaseIgnition();

	UFUNCTION()
	void OnRep_EngineIgnitionState(EForgeEngineIgnitionState PreviousState);

public:

	/* Called when the Ignition State has changed. */
	UPROPERTY(BlueprintAssignable, Category = "Engine Ignition")
	FForgeEngineIgnitionStateChangedDelegate OnEngineIgnitionStateChanged;

	/* Called when Engaging or Disengaging the Engine Ignition was released. */
	UPROPERTY(BlueprintAssignable, Category = "Engine Ignition")
	FForgeEngineReleasedIgnitionDelegate OnEngineReleasedIgnition;

	/* Called when the cooldown for Engine Ignition has elapsed. */
	UPROPERTY(BlueprintAssignable, Category = "Engine Ignition")
	FForgeEngineIgnitionCooldownElapsedDelegate OnEngineIgnitionCooldownElapsed;

	/* Sets whether or not the Engine is already started. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Engine Ignition")
	bool bEngineAlreadyStarted;

protected:

	/* How long it takes to start the Engine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (UIMin = 0, ClampMin = 0), Category = "Engine Ignition")
	float EngineStartTime;

	/* How long it takes to stop the Engine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (UIMin = 0, ClampMin = 0), Category = "Engine Ignition")
	float EngineStopTime;

	/* How long before you can modify Engine Ignition state again. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (UIMin = 0, ClampMin = 0), Category = "Engine Ignition")
	float EngineIgnitionCooldown;

	/* The state of ignition for the Engine. */
	UPROPERTY(Replicated, ReplicatedUsing = "OnRep_EngineIgnitionState")
	EForgeEngineIgnitionState EngineIgnitionState;

	/* Timer Handle that manages the transition for starting the Engine. */
	UPROPERTY()
	FTimerHandle EngagingIgnitionHandle;

	/* Timer Handle that manages the transition for shutting down the Engine. */
	UPROPERTY()
	FTimerHandle DisengagingIgnitionHandle;

	/* Timer Handle restricts changing state of the Engine Ignition until its elapsed. */
	UPROPERTY()
	FTimerHandle IgnitionCooldownHandle;
};
