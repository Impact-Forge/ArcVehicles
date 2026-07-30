// Copyright Impact-Forge. One relayed frame of pilot input.

#pragma once

#include "CoreMinimal.h"

#include "ForgeDroneInputFrame.generated.h"

/**
 * A whole frame of stick input, sent as one unreliable message.
 *
 * Four separate reliable per-axis RPCs is the obvious way to do this and the wrong one: it costs four
 * times the messages, and reliability is worthless for a value that is about to be superseded a
 * thirtieth of a second later. Sending the complete control state unreliably instead means a lost
 * packet costs one frame of staleness rather than a stalled channel.
 *
 * Axes are quantised to a byte each. A thumbstick has nowhere near eight bits of meaningful precision,
 * and this struct goes out tens of times a second per drone in flight.
 */
USTRUCT()
struct FForgeDroneInputFrame
{
	GENERATED_BODY()

	/* Forward/back. Pitch on a multirotor, elevator on a wing. */
	UPROPERTY()
	int8 Longitudinal = 0;

	/* Left/right roll. */
	UPROPERTY()
	int8 Lateral = 0;

	/* Yaw / rudder. */
	UPROPERTY()
	int8 Yaw = 0;

	/* Collective on a multirotor, engine throttle on a wing. */
	UPROPERTY()
	int8 Vertical = 0;

	UPROPERTY()
	int8 GimbalPitch = 0;

	UPROPERTY()
	int8 GimbalYaw = 0;

	/**
	 * Wrapping frame counter. Unreliable delivery means frames can arrive out of order, and applying an
	 * older one after a newer one would jerk the aircraft backwards through inputs the pilot has
	 * already left behind.
	 */
	UPROPERTY()
	uint8 Sequence = 0;

	/* Quantise a normalised [-1, 1] axis into a byte. */
	static int8 Quantise(const float Value)
	{
		return static_cast<int8>(FMath::RoundToInt(FMath::Clamp(Value, -1.f, 1.f) * 127.f));
	}

	/* Back to [-1, 1]. */
	static float Dequantise(const int8 Value)
	{
		return FMath::Clamp(static_cast<float>(Value) / 127.f, -1.f, 1.f);
	}

	/**
	 * Whether this frame is newer than SequenceToBeat, accounting for the counter wrapping at 256.
	 * Signed-difference comparison, so 2 correctly reads as newer than 250.
	 */
	bool IsNewerThan(const uint8 SequenceToBeat) const
	{
		// Narrow to uint8 before reinterpreting as signed, so the wrap is plain modular arithmetic
		// rather than a conversion of an out-of-range int.
		const uint8 Difference = static_cast<uint8>(Sequence - SequenceToBeat);
		return static_cast<int8>(Difference) > 0;
	}
};

/** Discrete pilot commands. Sent reliably, because unlike a stick position these do not repeat. */
UENUM(BlueprintType)
enum class EForgeDroneRelayCommand : uint8
{
	ArmMotors,
	DisarmMotors,
	ToggleFlightMode,
	ReleaseStore,
	ArmWarhead,
	ReturnToHome,
	HoldPosition,
	ResumeManual
};
