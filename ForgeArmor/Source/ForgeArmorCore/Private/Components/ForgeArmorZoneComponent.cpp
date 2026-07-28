// Copyright Impact-Forge. Armor zone component implementation.

#include "Components/ForgeArmorZoneComponent.h"

namespace ForgeArmor
{
	// Terminal Ballistics' plain component tag for "stop the projectile here".
	static FName GImpenetrableTagName(TEXT("IMPENETRABLE"));

	FName GetImpenetrableTagName()
	{
		return GImpenetrableTagName;
	}

	void SetImpenetrableTagName(const FName& TagName)
	{
		if (!TagName.IsNone())
		{
			GImpenetrableTagName = TagName;
		}
	}
}

UForgeArmorZoneComponent::UForgeArmorZoneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	BodyInstance.bNotifyRigidBodyCollision = false;
}

void UForgeArmorZoneComponent::SetArmed(const bool bArmed)
{
	const FName TagName = ForgeArmor::GetImpenetrableTagName();
	if (bArmed)
	{
		ComponentTags.AddUnique(TagName);
	}
	else
	{
		ComponentTags.Remove(TagName);
	}
}

bool UForgeArmorZoneComponent::IsArmed() const
{
	return ComponentTags.Contains(ForgeArmor::GetImpenetrableTagName());
}

void UForgeArmorZoneComponent::ConfigureZoneCollision(const ECollisionChannel ProjectileChannel)
{
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ProjectileChannel, ECR_Block);
}

void UForgeArmorZoneComponent::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (MaterialPhysMatTag.IsValid())
	{
		TagContainer.AddTag(MaterialPhysMatTag);
	}
}
