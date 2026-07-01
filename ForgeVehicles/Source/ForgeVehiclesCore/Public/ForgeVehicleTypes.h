// Copyright 2017-2020 Puny Human, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ForgeVehicleTypes.generated.h"

class AForgeBaseVehicle;
class UForgeVehicleSeatConfig;
class UAnimInstance;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FORGEVEHICLESCORE_API FForgeOwnerAttachmentReference
{
	GENERATED_USTRUCT_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
		FName ComponentName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
		FName SocketName;

	class USceneComponent* GetSceneComponent(AActor* OwnerActor);
};

UENUM(BlueprintType)
enum class EForgeVehicleSeatChangeType : uint8
{
	Invalid,
	EnterVehicle,
	ExitVehicle,
	SwitchSeats
};

struct FORGEVEHICLESCORE_API FForgeVehicleScopedRelativeTransformRestoration
{
	FForgeVehicleScopedRelativeTransformRestoration();
	FForgeVehicleScopedRelativeTransformRestoration(AActor* InActor);
	~FForgeVehicleScopedRelativeTransformRestoration();

	void Restore();

	TWeakObjectPtr<class AActor> OwnerActor;
	TMap<TWeakObjectPtr<class USceneComponent>, FTransform> ComponentTransformMap;
};

USTRUCT(BlueprintType)
struct FORGEVEHICLESCORE_API FForgeVehicleSeatReference
{
	GENERATED_USTRUCT_BODY()
public:
	FForgeVehicleSeatReference()
		: Vehicle(nullptr),
		SeatID(INDEX_NONE)
	{

	}

	FForgeVehicleSeatReference(AForgeBaseVehicle* InVehicle, int32 InSeatId)
		: Vehicle(InVehicle)
		, SeatID(InSeatId)
	{

	}

	FForgeVehicleSeatReference(UForgeVehicleSeatConfig* SeatConfig);

	friend class AForgeBaseVehicle;

	UPROPERTY()
	AForgeBaseVehicle* Vehicle;
	UPROPERTY()
	int32 SeatID;

	UForgeVehicleSeatConfig* operator->();
	UForgeVehicleSeatConfig* operator*();

	bool IsValid() const;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
	FString ToString() const;
};

template<>
struct TStructOpsTypeTraits<FForgeVehicleSeatReference> : public TStructOpsTypeTraitsBase2<FForgeVehicleSeatReference>
{
	enum
	{
		WithNetSerializer = true,
	};
};

/** Optional per-axis clamp applied to an occupant's view while seated. */
USTRUCT(BlueprintType)
struct FORGEVEHICLESCORE_API FForgeAxisViewRestriction
{
	GENERATED_BODY()

public:

	/* Whether this axis is clamped at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewRestriction")
	bool bRestricted = false;

	/* Minimum allowed angle (degrees, relative to the seat's forward) when bRestricted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewRestriction", meta = (EditCondition = "bRestricted"))
	float MinAngle = -80.f;

	/* Maximum allowed angle (degrees, relative to the seat's forward) when bRestricted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ViewRestriction", meta = (EditCondition = "bRestricted"))
	float MaxAngle = 80.f;
};

/**
 * Game-agnostic per-seat presentation/behaviour data, generalised from the BurningLands FBLSeatData.
 * A vehicle keys this by seat config (driver / additional seats) so that entering a seat can drive the
 * occupant's input context, held-item visibility, seated animation layer, camera and view restrictions.
 */
USTRUCT(BlueprintType)
struct FORGEVEHICLESCORE_API FForgeSeatData
{
	GENERATED_BODY()

public:

	/* Enhanced-Input mapping context to add for the occupant while in this seat. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeSeat|Input")
	TSoftObjectPtr<UInputMappingContext> InputMappingContext = nullptr;

	/* Priority of the mapping context above. Higher wins on conflicting binds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeSeat|Input")
	int32 InputMappingPriority = 0;

	/* Whether the occupant's held item/weapon is visible while seated. Usually false for drivers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeSeat")
	bool bShowHeldItemInSeat = true;

	/* Animation layer linked into the occupant's animation blueprint while seated. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeSeat")
	TSubclassOf<UAnimInstance> CharacterAnimationSet = nullptr;

	/* Camera component name. Optional standalone camera for this seat; takes priority over CameraSocket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeSeat|Camera")
	FName CameraComponentName = NAME_None;

	/* Camera socket name on the vehicle mesh. Optional custom camera position for this seat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeSeat|Camera")
	FName CameraSocket = NAME_None;

	/* Pitch view clamp applied to the occupant while in this seat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeSeat|ViewRestrictions")
	FForgeAxisViewRestriction PitchViewRestriction;

	/* Yaw view clamp applied to the occupant while in this seat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ForgeSeat|ViewRestrictions")
	FForgeAxisViewRestriction YawViewRestriction;
};