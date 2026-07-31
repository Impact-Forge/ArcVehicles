// Copyright Impact-Forge. Base vehicle ability implementation.

#include "GAS/ForgeVehicleAbility.h"

#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "ForgeVehicle.h"
#include "GameFramework/Actor.h"

UForgeVehicleAbility::UForgeVehicleAbility()
{
	// Instanced per actor so each vehicle's granted instance keeps its own SourceObject, and
	// server-initiated because every meaningful vehicle state change is authority-owned.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

AForgeVehicle* UForgeVehicleAbility::GetOwningVehicle() const
{
	if (const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		if (AForgeVehicle* SourceVehicle = Cast<AForgeVehicle>(Spec->SourceObject.Get()))
		{
			return SourceVehicle;
		}
	}
	return Cast<AForgeVehicle>(GetAvatarActorFromActorInfo());
}

void UForgeVehicleAbility::DisableCollisionForAll(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	Actor->SetActorEnableCollision(false);
	Actor->SetReplicateMovement(false);
	Actor->DisableComponentsSimulatePhysics();

	TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (IsValid(Primitive))
		{
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	TArray<AActor*> AttachedActors;
	Actor->GetAttachedActors(AttachedActors);
	for (AActor* Attached : AttachedActors)
	{
		DisableCollisionForAll(Attached);
	}
}

void UForgeVehicleAbility::GrantSpecificAbilities(const FGameplayAbilityActorInfo* ActorInfo, UObject* SourceObject, const TArray<TSubclassOf<UGameplayAbility>>& AbilitiesToGrant)
{
	if (!ActorInfo || !SourceObject)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& Ability : AbilitiesToGrant)
	{
		if (!IsValid(Ability))
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(Ability, 1, INDEX_NONE, ActorInfo->AvatarActor.Get());
		AbilitySpec.SourceObject = SourceObject;
		ASC->GiveAbility(AbilitySpec);
	}
}
