// Copyright Impact-Forge. Control link implementation.

#include "Systems/ForgeDroneLinkComponent.h"

#include "Engine/World.h"
#include "ForgeVehicle.h"
#include "ForgeVehiclesDrones.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "Interfaces/ForgeVehicleMovementInterface.h"
#include "Math/ForgeDroneMath.h"
#include "Net/UnrealNetwork.h"
#include "Systems/ForgeDroneAutopilotComponent.h"
#include "Systems/ForgeDroneJammerComponent.h"

UForgeDroneLinkComponent::UForgeDroneLinkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UForgeDroneLinkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeDroneLinkComponent, LinkQuality);
}

void UForgeDroneLinkComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner())
	{
		HomeLocation = GetOwner()->GetActorLocation();
	}
}

void UForgeDroneLinkComponent::SetOperator(AController* InOperatorController, AActor* InOperatorAntenna)
{
	OperatorController = InOperatorController;
	OperatorAntenna = InOperatorAntenna;

	// A freshly bound link starts healthy: the operator is standing next to the drone they just
	// launched, and starting degraded would fire a spurious failsafe.
	LinkQuality = 1.f;
	LineOfSightFactor = 1.f;
	TimeLinkLost = 0.f;
	bLinkLostReported = false;

	if (bFailsafeEngaged)
	{
		ReleaseFailsafe();
	}
}

void UForgeDroneLinkComponent::ClearOperator()
{
	OperatorController = nullptr;
	OperatorAntenna = nullptr;
}

float UForgeDroneLinkComponent::GetOperatorDistanceM() const
{
	const AActor* Antenna = OperatorAntenna.Get();
	if (!Antenna || !GetOwner())
	{
		return -1.f;
	}
	return FVector::Dist(GetOwner()->GetActorLocation(), Antenna->GetActorLocation()) / 100.f;
}

void UForgeDroneLinkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Decay the per-axis hold windows on every machine that filters input.
	for (int32 Axis = 0; Axis < NumFilteredAxes; ++Axis)
	{
		AxisHoldRemaining[Axis] = FMath::Max(AxisHoldRemaining[Axis] - DeltaTime, 0.f);
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// Evaluation is rate-limited: each pass costs a line trace, and link conditions change on the
	// scale of a drone flying behind a building, not a frame.
	TimeSinceEvaluation += DeltaTime;
	const float EvaluationInterval = 1.f / FMath::Max(EvaluationHz, 1.f);
	if (TimeSinceEvaluation >= EvaluationInterval)
	{
		TimeSinceEvaluation = 0.f;
		EvaluateLink();
	}

	// Smoothing runs every frame so quality moves continuously between evaluations.
	const float PreviousQuality = LinkQuality;

	if (IsLinkLost())
	{
		TimeLinkLost += DeltaTime;
		if (!bLinkLostReported)
		{
			bLinkLostReported = true;
			OnLinkLost.Broadcast();
			UE_LOG(LogForgeDrones, Verbose, TEXT("%s lost its control link."), *GetNameSafe(GetOwner()));
		}
		if (!bFailsafeEngaged && TimeLinkLost >= LinkLossGraceSeconds)
		{
			EngageFailsafe();
		}
	}
	else
	{
		if (bLinkLostReported)
		{
			bLinkLostReported = false;
			OnLinkRegained.Broadcast();
		}
		if (bFailsafeEngaged)
		{
			ReleaseFailsafe();
		}
		TimeLinkLost = 0.f;
	}

	if (!FMath::IsNearlyEqual(PreviousQuality, LinkQuality, 0.01f))
	{
		OnLinkQualityChanged.Broadcast(LinkQuality);
	}
}

void UForgeDroneLinkComponent::EvaluateLink()
{
	using namespace ForgeDrone::Link;

	const AActor* Owner = GetOwner();
	const AActor* Antenna = OperatorAntenna.Get();

	if (!Owner)
	{
		return;
	}

	// No operator means nothing is commanding the aircraft. Report a dead link so an abandoned drone
	// (its pilot killed, or control released mid-flight) runs its failsafe rather than hanging.
	if (!Antenna)
	{
		LinkQuality = 0.f;
		return;
	}

	const FVector DronePosition = Owner->GetActorLocation();
	const FVector OperatorPosition = Antenna->GetActorLocation();

	const float DistanceM = FVector::Dist(DronePosition, OperatorPosition) / 100.f;
	const float Range = RangeFactor(DistanceM, MaxRangeM, FullQualityRangeFraction);

	// Line of sight is smoothed rather than applied raw: a lamp post crossing the path should not
	// register as a link failure.
	const float TargetLineOfSight = EvaluateLineOfSight();
	const float EvaluationInterval = 1.f / FMath::Max(EvaluationHz, 1.f);
	LineOfSightFactor = SmoothTowards(LineOfSightFactor, TargetLineOfSight, QualitySmoothingPerSecond, EvaluationInterval);

	float Jam = 0.f;
	if (const UForgeDroneJammerSubsystem* Jammers = UForgeDroneJammerSubsystem::Get(this))
	{
		Jam = Jammers->ComputeJamFactor(DronePosition, OperatorPosition, BandTag);
	}

	const float TargetQuality = CombineQuality(Range, LineOfSightFactor, Jam);
	LinkQuality = SmoothTowards(LinkQuality, TargetQuality, QualitySmoothingPerSecond, EvaluationInterval);
}

float UForgeDroneLinkComponent::EvaluateLineOfSight() const
{
	const AActor* Owner = GetOwner();
	const AActor* Antenna = OperatorAntenna.Get();
	if (!Owner || !Antenna || !GetWorld())
	{
		return 1.f;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ForgeDroneLinkLOS), /*bTraceComplex*/ false);
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.AddIgnoredActor(Antenna);

	// A single visibility trace: terrain and buildings attenuate the signal, foliage and thin cover
	// generally will not block this channel.
	const bool bBlocked = GetWorld()->LineTraceTestByChannel(
		Owner->GetActorLocation(),
		Antenna->GetActorLocation(),
		ECC_Visibility,
		QueryParams);

	return bBlocked ? FMath::Clamp(OcclusionFactor, 0.f, 1.f) : 1.f;
}

void UForgeDroneLinkComponent::EngageFailsafe()
{
	bFailsafeEngaged = true;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UForgeDroneAutopilotComponent* Autopilot = Owner->FindComponentByClass<UForgeDroneAutopilotComponent>();

	switch (LinkLossBehavior)
	{
	case EForgeDroneLinkLossBehavior::Continue:
		// Deliberately nothing: a committed terminal dive should finish on its last commands.
		break;

	case EForgeDroneLinkLossBehavior::FailsafeHover:
		if (Autopilot)
		{
			Autopilot->SetModeFromFailsafe(EForgeDroneAutopilotMode::PositionHold);
		}
		break;

	case EForgeDroneLinkLossBehavior::ReturnToHome:
		if (Autopilot)
		{
			Autopilot->SetHomeLocation(HomeLocation);
			Autopilot->SetModeFromFailsafe(EForgeDroneAutopilotMode::ReturnToHome);
		}
		break;

	case EForgeDroneLinkLossBehavior::CutMotors:
		if (AForgeVehicle* Vehicle = Cast<AForgeVehicle>(Owner))
		{
			if (TScriptInterface<IForgeVehicleMovementInterface> Movement = Vehicle->GetVehicleMovementInterface())
			{
				Movement->StopEngine();
			}
		}
		break;

	default:
		break;
	}

	UE_LOG(LogForgeDrones, Verbose, TEXT("%s engaged link-loss failsafe."), *GetNameSafe(Owner));
}

void UForgeDroneLinkComponent::ReleaseFailsafe()
{
	bFailsafeEngaged = false;

	// Only hand control back if the failsafe is what took it. A pilot-commanded orbit or dive should
	// survive the link flickering.
	if (AActor* Owner = GetOwner())
	{
		if (UForgeDroneAutopilotComponent* Autopilot = Owner->FindComponentByClass<UForgeDroneAutopilotComponent>())
		{
			if (Autopilot->WasEngagedByFailsafe())
			{
				Autopilot->SetMode(EForgeDroneAutopilotMode::Manual);
			}
		}
	}
}

float UForgeDroneLinkComponent::FilterInput(const int32 AxisIndex, const float RawValue)
{
	if (AxisIndex < 0 || AxisIndex >= NumFilteredAxes)
	{
		return RawValue;
	}

	// A healthy link is transparent.
	if (LinkQuality >= DegradedQualityThreshold)
	{
		HeldAxisValues[AxisIndex] = RawValue;
		AxisHoldRemaining[AxisIndex] = 0.f;
		return RawValue;
	}

	// Below the threshold, how badly the link is degraded scales from 0 to 1 as quality falls from the
	// threshold to fully lost.
	const float Span = FMath::Max(DegradedQualityThreshold - LostQualityThreshold, UE_SMALL_NUMBER);
	const float Degradation = FMath::Clamp((DegradedQualityThreshold - LinkQuality) / Span, 0.f, 1.f);

	// Still inside a hold window: the aircraft keeps acting on the last command that arrived.
	if (AxisHoldRemaining[AxisIndex] > 0.f)
	{
		return HeldAxisValues[AxisIndex];
	}

	// Otherwise the command gets through, and a new dropout window opens with probability scaled by
	// how bad the link is. Packets are lost in bursts, not smoothly.
	if (FMath::FRand() < Degradation)
	{
		AxisHoldRemaining[AxisIndex] = Degradation * MaxInducedLatencySeconds;
		return HeldAxisValues[AxisIndex];
	}

	HeldAxisValues[AxisIndex] = RawValue;
	return RawValue;
}

void UForgeDroneLinkComponent::OnRep_LinkQuality()
{
	// Clients drive video static, HUD warnings and audio from this.
	OnLinkQualityChanged.Broadcast(LinkQuality);
}
