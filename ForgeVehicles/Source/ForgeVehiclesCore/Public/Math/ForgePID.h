// Copyright Impact-Forge. Reusable PID controller shared by vehicle autopilots and stability loops.

#pragma once

#include "CoreMinimal.h"

#include "ForgePID.generated.h"

/**
 * Plain PID controller with the same tuning surface as the ground-vehicle spline AI
 * (Kp / Kd / Kd2 / Ki / MaxIntegral / OutputMax / MinDeltaTime), lifted out so flight
 * stability loops and drone autopilots can reuse it without depending on the ground module.
 *
 * Header-only and UObject-free: state lives in the struct, so an owner can hold one instance
 * per controlled axis. Not thread-safe; call Update from a single context.
 */
USTRUCT(BlueprintType)
struct FForgePIDController
{
	GENERATED_BODY()

	/* Proportional gain. Raises responsiveness; too high oscillates and overshoots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID", meta = (ClampMin = "0.0"))
	float Kp = 0.3f;

	/* Derivative gain. Damps the rate of change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID", meta = (ClampMin = "0.0"))
	float Kd = 0.2f;

	/* Second-derivative gain. Smooths the system as a whole; often more useful than Kd. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID")
	float Kd2 = 0.0f;

	/* Integral gain. Corrects steady-state offset (e.g. holding altitude against gravity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID", meta = (ClampMin = "0.0"))
	float Ki = 0.0f;

	/* Anti-windup clamp on the accumulated integral term. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID", meta = (ClampMin = "0.0"))
	float MaxIntegral = 20.0f;

	/* Symmetric clamp applied to the returned output. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID", meta = (ClampMin = "0.0"))
	float OutputMax = 1.0f;

	/* Below this delta time the derivative/integral terms are skipped (avoids divide-by-zero blowups). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PID", meta = (ClampMin = "0.0"))
	float MinDeltaTime = 0.0001f;

	/**
	 * Advance the controller.
	 * @param Error      Setpoint minus measurement, in whatever unit the axis uses.
	 * @param DeltaTime  Seconds since the previous call.
	 * @return           Control output, clamped to +/- OutputMax.
	 */
	float Update(float Error, float DeltaTime)
	{
		float Derivative = 0.f;
		float SecondDerivative = 0.f;

		if (DeltaTime > MinDeltaTime)
		{
			Derivative = (Error - PreviousError) / DeltaTime;
			SecondDerivative = (Derivative - PreviousDerivative) / DeltaTime;

			Integral = FMath::Clamp(Integral + Error * DeltaTime, -MaxIntegral, MaxIntegral);

			PreviousDerivative = Derivative;
		}

		PreviousError = Error;

		const float Output = Kp * Error + Kd * Derivative + Kd2 * SecondDerivative + Ki * Integral;
		return FMath::Clamp(Output, -OutputMax, OutputMax);
	}

	/* Clears accumulated state. Call when re-engaging a loop or retargeting, to avoid a kick. */
	void Reset()
	{
		Integral = 0.f;
		PreviousError = 0.f;
		PreviousDerivative = 0.f;
	}

private:

	float Integral = 0.f;
	float PreviousError = 0.f;
	float PreviousDerivative = 0.f;
};
