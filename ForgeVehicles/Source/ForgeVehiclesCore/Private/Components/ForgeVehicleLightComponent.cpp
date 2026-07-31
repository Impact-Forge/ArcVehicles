// Copyright Impact-Forge. Vehicle light component implementation.

#include "Components/ForgeVehicleLightComponent.h"

#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

UForgeVehicleLightComponent::UForgeVehicleLightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UForgeVehicleLightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UForgeVehicleLightComponent, bIsOn);
}

void UForgeVehicleLightComponent::BeginPlay()
{
	Super::BeginPlay();

	GatherDrivenComponents();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bIsOn = bStartsOn;
	}

	ApplyLightState();
}

void UForgeVehicleLightComponent::GatherDrivenComponents()
{
	DrivenLights.Reset();
	DrivenMeshes.Reset();
	DefaultIntensities.Reset();

	TArray<USceneComponent*> Children;
	GetChildrenComponents(/*bIncludeAllDescendants*/ true, Children);

	for (USceneComponent* Child : Children)
	{
		if (ULightComponent* Light = Cast<ULightComponent>(Child))
		{
			DrivenLights.Add(Light);
			DefaultIntensities.Add(Light->Intensity);
		}
		else if (UMeshComponent* Mesh = Cast<UMeshComponent>(Child))
		{
			DrivenMeshes.Add(Mesh);
		}
	}
}

void UForgeVehicleLightComponent::SetLightOn(const bool bNewOn)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bIsOn == bNewOn)
	{
		return;
	}

	bIsOn = bNewOn;
	ApplyLightState();
}

void UForgeVehicleLightComponent::OnRep_IsOn()
{
	ApplyLightState();
}

void UForgeVehicleLightComponent::ApplyLightState()
{
	for (int32 Index = 0; Index < DrivenLights.Num(); ++Index)
	{
		ULightComponent* Light = DrivenLights[Index];
		if (!IsValid(Light))
		{
			continue;
		}

		Light->SetVisibility(bIsOn);
		if (bIsOn && OnIntensity >= 0.f)
		{
			Light->SetIntensity(OnIntensity);
		}
		else if (bIsOn && DefaultIntensities.IsValidIndex(Index))
		{
			Light->SetIntensity(DefaultIntensities[Index]);
		}
	}

	if (!EmissiveParameterName.IsNone())
	{
		const float EmissiveValue = bIsOn ? 1.f : 0.f;
		for (UMeshComponent* Mesh : DrivenMeshes)
		{
			if (!IsValid(Mesh))
			{
				continue;
			}
			const int32 MaterialCount = Mesh->GetNumMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				if (UMaterialInstanceDynamic* Dynamic = Mesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
				{
					Dynamic->SetScalarParameterValue(EmissiveParameterName, EmissiveValue);
				}
			}
		}
	}

	OnLightChanged.Broadcast(this, bIsOn);
}
