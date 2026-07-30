// Copyright Impact-Forge. Handing a player control of a drone, and getting them back afterwards.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Operator/ForgeDroneInputFrame.h"

#include "ForgeDroneOperatorComponent.generated.h"

class AController;
class APlayerController;
class APlayerState;
class UForgeDroneLinkComponent;

/** Why a control request was refused, so callers can say something useful instead of just failing. */
UENUM(BlueprintType)
enum class EForgeDroneControlResult : uint8
{
	/* Control granted; the operator is now flying the drone. */
	Granted,
	/* Someone else is already flying it. */
	AlreadyControlled,
	/* The controller has no pawn to leave behind, so there would be nothing to return to. */
	NoOperatorBody,
	/* Null or already-destroyed controller. */
	InvalidOperator,
	/* Called on a client, or the drone is being torn down. */
	Unavailable
};

/** How the operator is flying it. */
UENUM(BlueprintType)
enum class EForgeDroneControlMode : uint8
{
	/* Unmanned. */
	None,
	/* Phase A: the operator possesses the drone and their body is left standing, unpossessed. */
	Possessed,
	/* Phase B: the operator keeps their body and their input is relayed to the drone. */
	Relayed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeDroneControlChanged, AController*, Operator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeDroneOperatorBodyLost, AController*, Operator);

/**
 * Lets a player fly this drone, and puts them back where they were afterwards.
 *
 * Two ways in, and the difference matters:
 *
 * - **Possession** (`TakeControl`). The controller possesses the drone; their body is left standing
 *   where it was, unpossessed. Simple, and the camera and input come along for free.
 * - **Relay** (`BeginRelayControl`). The controller keeps their body and their stick input is relayed
 *   to the drone as packed frames, with the view target pointed at the aircraft. Nothing is ever
 *   unpossessed, so the whole possession/input-teardown path is sidestepped, the body stays fully
 *   under its owner's control, and the soldier can be made to visibly stand there holding a
 *   controller.
 *
 * Neither is the seat system, deliberately. A seat attaches its occupant to the vehicle, hides them
 * and disables their movement, which is right for a driver and wrong for a drone operator: the
 * soldier stays standing exactly where they are, in the open, visible and shootable. That
 * vulnerability is the point - it is the cost of using a drone - so nothing here moves, hides or
 * protects the operator's body.
 *
 * The body is also where the radio is. It is passed to UForgeDroneLinkComponent as the antenna, so
 * range, terrain occlusion and jamming are all measured from the soldier rather than from the drone's
 * own position - which is what makes flying deep behind a ridge a decision rather than a free move.
 */
UCLASS(ClassGroup = (ForgeDrone), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESDRONES_API UForgeDroneOperatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeDroneOperatorComponent();

	//~ Begin UActorComponent interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	//~ Phase A: possession ---------------------------------------------------

	/**
	 * Hand control of this drone to a controller by possession, leaving their pawn behind in the world.
	 *
	 * Server only, and deliberately not a client RPC: before control is taken the drone is not owned
	 * by the operator's connection, so a request from that client would simply be dropped. Call this
	 * from whatever the player *does* own - a granted ability, an interaction on a deployed drone, or
	 * the item that carries it.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Operator")
	EForgeDroneControlResult TakeControl(AController* NewOperator);

	//~ Phase B: relay --------------------------------------------------------

	/**
	 * Hand control to a controller without possessing anything. Their pawn stays theirs; the drone
	 * becomes owned by their connection so it can receive their input frames, and their view target is
	 * pointed at it.
	 *
	 * Server only, for the same reason as TakeControl.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Operator")
	EForgeDroneControlResult BeginRelayControl(AController* NewOperator);

	/**
	 * Set the pilot's current stick positions, on the operator's own client. Accumulated into a pending
	 * frame and flushed to the server at RelaySendHz rather than sent per call, so how often the
	 * project chooses to push input does not change how much network traffic a drone costs.
	 *
	 * Values are normalised [-1, 1]; Vertical is collective on a multirotor and engine throttle on a
	 * wing, matching the movement interface the autopilot drives.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Operator|Relay")
	void SetRelayFlightInput(float Longitudinal, float Lateral, float InYaw, float Vertical);

	/* As above, for the camera. Ignored if the drone has no gimbal. */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Operator|Relay")
	void SetRelayGimbalInput(float GimbalPitch, float GimbalYaw);

	/**
	 * Send a discrete command. Reliable, because unlike a stick position these do not repeat - a lost
	 * "release store" is not corrected by the next frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Operator|Relay")
	void SendRelayCommand(EForgeDroneRelayCommand Command);

	//~ Release --------------------------------------------------------------

	/**
	 * End control, by whichever route it was taken. Puts a possessing operator back in their body and
	 * restores a relaying operator's view target, and leaves the drone unmanned either way.
	 *
	 * Returns false if nobody was flying it.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Operator")
	bool ReleaseControl();

	/**
	 * Client-callable release. Valid where the take-control calls are not: both routes make the drone
	 * owned by the operator's connection, so by the time there is something to release, the RPC has a
	 * route.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ForgeDrone|Operator")
	void ServerRequestRelease();

	//~ State ----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	EForgeDroneControlMode GetControlMode() const { return ControlMode; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	bool IsControlled() const { return ControlMode != EForgeDroneControlMode::None; }

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	bool IsRelaying() const { return ControlMode == EForgeDroneControlMode::Relayed; }

	/* The controller flying the drone. Authority only - controllers do not replicate to other clients. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	AController* GetOperatorController() const { return OperatorController.Get(); }

	/* The operator's own pawn, standing wherever they left it. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	APawn* GetOperatorBody() const { return OperatorBody.Get(); }

	/* Who is flying it, replicated to everyone, so any client can label the drone. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	APlayerState* GetOperatorPlayerState() const { return OperatorPlayerState; }

	/* True on the client whose player is flying this drone, by either route. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	bool IsLocalOperator() const;

	//~ Tunables -------------------------------------------------------------

	/**
	 * Whether losing the operator's body also takes the drone away from them.
	 *
	 * True is the realistic default: the radio is on the soldier, so their death is the end of the
	 * flight. A possessing operator is left without a pawn, exactly as they would be for any other
	 * death, and the project's death handling takes it from there - which is why this is a choice
	 * rather than an assumption.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeDrone|Operator")
	bool bReleaseControlOnOperatorBodyLost = true;

	/* How many input frames a relaying client sends per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeDrone|Operator|Relay", meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float RelaySendHz = 30.f;

	/* Camera blend when a relaying operator's view moves to the drone and back, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeDrone|Operator|Relay", meta = (ClampMin = "0.0"))
	float RelayViewBlendSeconds = 0.4f;

	/**
	 * How long the server keeps applying the last frame it received before treating the stream as
	 * stalled and centring the sticks, seconds. Without this a dropped connection leaves the aircraft
	 * flying its final input indefinitely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeDrone|Operator|Relay", meta = (ClampMin = "0.1"))
	float RelayInputTimeoutSeconds = 1.f;

	//~ Events ---------------------------------------------------------------

	/* Fired on the server when an operator takes the drone, by either route. */
	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Operator")
	FOnForgeDroneControlChanged OnControlTaken;

	/* Fired on the server when the drone is released, for any reason including its own destruction. */
	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Operator")
	FOnForgeDroneControlChanged OnControlReleased;

	/**
	 * Fired when the operator's body is destroyed while they are flying. The controller passed may
	 * have no pawn by the time this runs; that is the situation being reported.
	 */
	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Operator")
	FOnForgeDroneOperatorBodyLost OnOperatorBodyLost;

	/* Fired on every machine when the control mode changes, for HUD and nameplate work. */
	UPROPERTY(BlueprintAssignable, Category = "ForgeDrone|Operator")
	FOnForgeDroneControlChanged OnControlModeChanged;

protected:

	//~ Relay internals

	/* Client to server: one frame of stick input. Unreliable by design; see FForgeDroneInputFrame. */
	UFUNCTION(Server, Unreliable)
	void ServerSendInputFrame(FForgeDroneInputFrame Frame);

	UFUNCTION(Server, Reliable)
	void ServerSendRelayCommand(EForgeDroneRelayCommand Command);

	/* Applies the most recent frame to the airframe, through the link's degradation. */
	void ApplyRelayInput(float DeltaTime);

	/* Runs a discrete command against whichever components the drone actually has. */
	void ExecuteRelayCommand(EForgeDroneRelayCommand Command);

	//~ Shared bookkeeping

	/* Common checks and operator recording for both control routes. */
	EForgeDroneControlResult BeginControl(AController* NewOperator, EForgeDroneControlMode Mode);

	/* Resolves the drone's link component, if it has one. */
	UForgeDroneLinkComponent* GetLink() const;

	/* The operator's controller as a player controller, for view-target work. */
	APlayerController* GetOperatorPlayerController() const;

	/* Clears operator bookkeeping and unbinds from the body. Does not possess anything. */
	void ClearOperatorState();

	/* Turns ticking on only while it is needed, on the machines that need it. */
	void UpdateTickEnabled();

	UFUNCTION()
	void HandleOperatorBodyDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnRep_ControlMode();

	UFUNCTION()
	void OnRep_OperatorPlayerState();

	/* Replicated to all clients: a player state does, where a controller does not. */
	UPROPERTY(ReplicatedUsing = OnRep_OperatorPlayerState)
	TObjectPtr<APlayerState> OperatorPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_ControlMode)
	EForgeDroneControlMode ControlMode = EForgeDroneControlMode::None;

	/* Authority-side only. Weak: the controller may be torn down by a disconnect mid-flight. */
	TWeakObjectPtr<AController> OperatorController;

	/* Authority-side only. Weak: the body can be destroyed while the drone is being flown. */
	TWeakObjectPtr<APawn> OperatorBody;

	/* Owner the drone had before a relay claimed it, so releasing puts it back. */
	TWeakObjectPtr<AActor> PreRelayOwner;

	//~ Relay state

	/* Client side: the frame being built up between sends. */
	FForgeDroneInputFrame PendingFrame;
	float TimeSinceFrameSent = 0.f;

	/* Server side: the newest frame received, and how long ago. */
	FForgeDroneInputFrame LastAppliedFrame;
	uint8 LastAppliedSequence = 0;
	bool bHasAppliedFrame = false;
	float TimeSinceFrameReceived = 0.f;
};
