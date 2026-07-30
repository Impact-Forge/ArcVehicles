// Copyright Impact-Forge. Handing a player control of a drone, and getting them back afterwards.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "ForgeDroneOperatorComponent.generated.h"

class AController;
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeDroneControlChanged, AController*, Operator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeDroneOperatorBodyLost, AController*, Operator);

/**
 * Lets a player fly this drone directly, by possessing it, and puts them back in their own body
 * afterwards.
 *
 * This is deliberately not the seat system. A seat attaches the occupant to the vehicle, hides them
 * and disables their movement, which is right for a driver and wrong for a drone operator: the
 * soldier stays standing where they are, in the open, visible and shootable, holding a controller.
 * That vulnerability is the point - it is the cost of using a drone - so nothing here moves, hides or
 * protects the operator's body. It only remembers it and hands the controller back to it later.
 *
 * The body is also where the radio is. It is passed to UForgeDroneLinkComponent as the antenna, so
 * range, terrain occlusion and jamming are all measured from the soldier rather than from the drone's
 * own position - which is what makes flying deep behind a ridge a decision rather than a free move.
 *
 * Phase A of the operator design: the player *is* the drone while flying it. Phase B relays input to
 * a drone the operator never possesses, keeping their body under their own control; both use this
 * component's bookkeeping.
 */
UCLASS(ClassGroup = (ForgeDrone), meta = (BlueprintSpawnableComponent))
class FORGEVEHICLESDRONES_API UForgeDroneOperatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UForgeDroneOperatorComponent();

	//~ Begin UActorComponent interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	/**
	 * Hand control of this drone to a controller, leaving their pawn behind in the world.
	 *
	 * Server only, and deliberately not a client RPC: before control is taken the drone is not owned
	 * by the operator's connection, so a request from that client would simply be dropped. Call this
	 * from whatever the player *does* own - a granted ability, an interaction on a deployed drone, or
	 * the item that carries it.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Operator")
	EForgeDroneControlResult TakeControl(AController* NewOperator);

	/**
	 * Put the operator back in their own body and leave the drone unmanned.
	 *
	 * Returns false if nobody was flying it. If the body is gone, the drone is still released - see
	 * bReleaseControlOnOperatorBodyLost for what happens to the operator in that case.
	 */
	UFUNCTION(BlueprintCallable, Category = "ForgeDrone|Operator")
	bool ReleaseControl();

	/**
	 * Client-callable release. Valid where TakeControl is not: possession makes the drone owned by the
	 * operator's connection, so by the time there is something to release, the RPC has a route.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "ForgeDrone|Operator")
	void ServerRequestRelease();

	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	bool IsControlled() const { return OperatorPlayerState != nullptr || OperatorController.IsValid(); }

	/* The controller flying the drone. Authority only - controllers do not replicate to other clients. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	AController* GetOperatorController() const { return OperatorController.Get(); }

	/* The operator's own pawn, standing wherever they left it. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	APawn* GetOperatorBody() const { return OperatorBody.Get(); }

	/* Who is flying it, replicated to everyone, so any client can label the drone. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	APlayerState* GetOperatorPlayerState() const { return OperatorPlayerState; }

	/* True on the client whose player is flying this drone. */
	UFUNCTION(BlueprintPure, Category = "ForgeDrone|Operator")
	bool IsLocalOperator() const;

	/**
	 * Whether losing the operator's body also takes the drone away from them.
	 *
	 * True is the realistic default: the radio is on the soldier, so their death is the end of the
	 * flight. The controller is left without a pawn, exactly as it would be for any other death, and
	 * the project's death handling takes it from there - which is why this is a choice rather than an
	 * assumption.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ForgeDrone|Operator")
	bool bReleaseControlOnOperatorBodyLost = true;

	/* Fired on the server when an operator takes the drone. */
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

protected:

	/* Resolves the drone's link component, if it has one. */
	UForgeDroneLinkComponent* GetLink() const;

	/* Clears operator bookkeeping and unbinds from the body. Does not possess anything. */
	void ClearOperatorState();

	UFUNCTION()
	void HandleOperatorBodyDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnRep_OperatorPlayerState();

	/* Replicated to all clients: a player state does, where a controller does not. */
	UPROPERTY(ReplicatedUsing = OnRep_OperatorPlayerState)
	TObjectPtr<APlayerState> OperatorPlayerState;

	/* Authority-side only. Weak: the controller may be torn down by a disconnect mid-flight. */
	TWeakObjectPtr<AController> OperatorController;

	/* Authority-side only. Weak: the body can be destroyed while the drone is being flown. */
	TWeakObjectPtr<APawn> OperatorBody;
};
