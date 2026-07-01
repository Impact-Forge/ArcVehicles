#pragma once

#include "CoreMinimal.h"
#include "ForgeVehicle.h"
#include "AsyncTickPawn.generated.h"

// Re-parented for Forge Vehicles: async-physics-ticking pawns are Forge vehicles, so every RTune
// ground vehicle inherits the Arc-derived seat / exit / ignition foundation from AForgeVehicle.
UCLASS(BlueprintType)
class ASYNCTICKPHYSICS_API AAsyncTickPawn : public AForgeVehicle
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "AsyncTick")
		void AsyncTick(float DeltaTime);
	
	virtual void NativeAsyncTick(float DeltaTime);

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};