#pragma once

#include <array>
#include "Plugin.h"
#include "IExamInterface.h"
#include "stdafx.h"

struct Inventory
{
	std::array<ItemInfo, 5> items{};
	std::array<bool, 5> slots{};

	UINT HasEmptySlot()
	{
		for (size_t i{}; i < slots.size(); i++)
		{
			if (slots[i] == false)
			{
				return i;
			}
		}

		return slots.size();
	}

	UINT DetermineUselessItemSlot(IExamInterface* examInterface, ItemInfo itemToConsider)
	{	
		switch (itemToConsider.Type)
		{
		case eItemType::PISTOL:

			{
				int newAmmoCount = examInterface->Weapon_GetAmmo(itemToConsider);
				auto hasItemOfTypeIt = HasTypeOfInInventory(itemToConsider.Type);

				// If contained get ammo and compare
				if (hasItemOfTypeIt != items.end())
				{
					int currentAmmo = examInterface->Weapon_GetAmmo(*hasItemOfTypeIt);

					if (newAmmoCount >= currentAmmo)
					{
						return hasItemOfTypeIt - items.begin();
					}
				}

				return items.size();
			}

			break;
		case eItemType::SHOTGUN:

			{
				int newAmmoCount = examInterface->Weapon_GetAmmo(itemToConsider);
				auto hasItemOfTypeIt = HasTypeOfInInventory(itemToConsider.Type);

				// If contained get ammo and compare
				if (hasItemOfTypeIt != items.end())
				{
					int currentAmmo = examInterface->Weapon_GetAmmo(*hasItemOfTypeIt);

					if (newAmmoCount >= currentAmmo)
					{
						return hasItemOfTypeIt - items.begin();
					}
				}

				return items.size();
			}

			break;

		case eItemType::FOOD:

			{
				int newFoodCount = examInterface->Food_GetEnergy(itemToConsider);
				auto hasItemOfTypeIt = HasTypeOfInInventory(itemToConsider.Type);

				// If contained get ammo and compare
				if (hasItemOfTypeIt != items.end())
				{
					int currentFood = examInterface->Food_GetEnergy(*hasItemOfTypeIt);

					if (newFoodCount >= currentFood)
					{
						return hasItemOfTypeIt - items.begin();
					}
				}

				return items.size();
			}

			break;

		case eItemType::MEDKIT:

			{
				int newMedkitCount = examInterface->Medkit_GetHealth(itemToConsider);
				auto hasItemOfTypeIt = HasTypeOfInInventory(itemToConsider.Type);

				// If contained get ammo and compare
				if (hasItemOfTypeIt != items.end())
				{
					int currentMedkitCount = examInterface->Medkit_GetHealth(*hasItemOfTypeIt);

					if (newMedkitCount >= currentMedkitCount)
					{
						return hasItemOfTypeIt - items.begin();
					}
				}

				return items.size();
			}

			break;

		case eItemType::GARBAGE:
			
			{
				
			}
			
			break;
		default:
			break;
		}
	}

	bool IsNewPickupBetterThanInventory(IExamInterface* examInterface, ItemInfo itemToConsider)
	{
		switch (itemToConsider.Type)
		{
		case eItemType::PISTOL:

		{
			int newAmmoCount = examInterface->Weapon_GetAmmo(itemToConsider);
			auto hasItemOfTypeIt = HasTypeOfInInventory(itemToConsider.Type);

			// If contained get ammo and compare
			if (hasItemOfTypeIt != items.end())
			{
				int currentAmmo = examInterface->Weapon_GetAmmo(*hasItemOfTypeIt);

				if (newAmmoCount > currentAmmo)
				{
					return true;
				}
			}

			return false;
		}

		break;
		case eItemType::SHOTGUN:

		{
			int newAmmoCount = examInterface->Weapon_GetAmmo(itemToConsider);
			auto hasItemOfTypeIt = HasTypeOfInInventory(itemToConsider.Type);

			// If contained get ammo and compare
			if (hasItemOfTypeIt != items.end())
			{
				int currentAmmo = examInterface->Weapon_GetAmmo(*hasItemOfTypeIt);

				if (newAmmoCount > currentAmmo)
				{
					return true;
				}
			}

			return false;
		}

		break;

		case eItemType::FOOD:

		{
			int newFoodCount = examInterface->Food_GetEnergy(itemToConsider);
			auto hasItemOfTypeIt = HasTypeOfInInventory(itemToConsider.Type);

			// If contained get ammo and compare
			if (hasItemOfTypeIt != items.end())
			{
				int currentFood = examInterface->Food_GetEnergy(*hasItemOfTypeIt);

				if (newFoodCount > currentFood)
				{
					return true;
				}
			}

			return false;
		}

		break;

		case eItemType::MEDKIT:

		{
			int newMedkitCount = examInterface->Medkit_GetHealth(itemToConsider);
			auto hasItemOfTypeIt = HasTypeOfInInventory(itemToConsider.Type);

			// If contained get ammo and compare
			if (hasItemOfTypeIt != items.end())
			{
				int currentMedkitCount = examInterface->Medkit_GetHealth(*hasItemOfTypeIt);

				if (newMedkitCount > currentMedkitCount)
				{
					return true;
				}
			}

			return false;
		}

		break;

		default:
			return false;
			break;
		}
	}

	UINT GetSameTypeItemSlot(eItemType itemType)
	{
		auto pistolIt = std::find_if(items.begin(), items.end(), [itemType](ItemInfo item) {
			return item.Type == itemType;
			});

		auto id = pistolIt - items.begin();

		if (id < slots.size() && slots[id] == false)
		{
			return items.size();
		}

		return id;
	}

	bool ShouldPickupItem(IExamInterface* examInterface, ItemInfo itemToConsider)
	{
		auto type = itemToConsider.Type;
		auto hasItemTypeIf = HasTypeOfInInventory(type);

		// Already has item
		if (hasItemTypeIf != items.end())
		{
			return IsNewPickupBetterThanInventory(examInterface, itemToConsider);
		}

		return true;
	}

	std::array<ItemInfo,5>::iterator HasTypeOfInInventory(eItemType type)
	{
		auto pistolIt = std::find_if(items.begin(), items.end(), [type](ItemInfo item) {
			return item.Type == type;
		});

		// default is pistol so we also need to check if the slot is actually filled;
		auto slotId = pistolIt - items.begin();
		if (slotId < slots.size() && slots[slotId] == false)
		{
			// invalid index gets caught
			return items.end();
		}

		return pistolIt;
	}
	
	void RemoveSlot(UINT slot)
	{
		auto nullItem = ItemInfo{};
		nullItem.Location = Elite::Vector2{ 0,0 };
		nullItem.Type = eItemType::_LAST;

		items[slot] = nullItem;
		slots[slot] = false;
	}

	void FillSlot(UINT slot, ItemInfo itemInfo)
	{
		items[slot] = itemInfo;
		slots[slot] = true;
	}
};

struct SweepHouse
{
	std::array<Elite::Vector2, CONFIG_MAX_HOUSE_SWEEP_SPOTS> sweepLocations{};

	int sweepIndex{};
	Elite::Vector2 activeSweepCenter{};

	void GenerateSweepLocations(HouseInfo house)
	{
		float housePercentage{.3f};
		int counter{};

		int increment = 360.f / (float)CONFIG_MAX_HOUSE_SWEEP_SPOTS;
		for (uint32_t i{}; i < 360; i += increment)
		{
			Elite::Vector2 location
			{
				(cos(Elite::ToRadians((float)i)) * house.Size.x * housePercentage) + house.Center.x,
				(sin(Elite::ToRadians((float)i)) * house.Size.y * housePercentage) + house.Center.y
			};

			sweepLocations[counter++] = location;
		}

		// reset sweepIndex
		activeSweepCenter = house.Center;
		sweepIndex = 0;
	}

	bool Sweep(Elite::Vector2 playerLocation)
	{
		float distance = Elite::DistanceSquared(playerLocation, sweepLocations[sweepIndex]);

		if (distance <= 1.0f)
		{
			sweepIndex++;
		}

		if (sweepIndex == CONFIG_MAX_HOUSE_SWEEP_SPOTS)
		{
			return true;
		}

		return false;
	}

	Elite::Vector2 GetNextSweepSpot() 
	{
		return sweepLocations[sweepIndex];
	}

	bool HasGeneratedLocations(Elite::Vector2 houseCenter)
	{
		return (Elite::DistanceSquared(activeSweepCenter, houseCenter) < FLT_EPSILON);
	}
};