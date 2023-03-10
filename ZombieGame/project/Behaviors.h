#include "EliteMath/EMath.h"
#include "EBehaviorTree.h"

#include "Plugin.h"
#include "IExamInterface.h"
#include "Structs.h"

void PrintMessage(std::string message)
{
	std::cout << "-----------------------" << "\n";
	std::cout << message << "\n";
	std::cout << "-----------------------" << "\n";
}

namespace BT_Actions
{
	BehaviorState DropOldGun(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		Inventory inventory{};
		IExamInterface* examInterface{};

		blackboard->GetData(P_ITEMS_IN_FOV, items);
		blackboard->GetData(P_INTERFACE, examInterface);
		blackboard->GetData(P_INVENTORY, inventory);

		auto item = items->front();

		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(item, itemInfo);

		if (!isItem)
		{
			return BehaviorState::Failure;
		}

		auto slot = inventory.GetSameTypeItemSlot(itemInfo.Type);
		examInterface->Inventory_RemoveItem(slot);

		inventory.RemoveSlot(slot);

		blackboard->ChangeData(P_INVENTORY, inventory);

		return BehaviorState::Success;
	}

	BehaviorState DestroyGun(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};

		blackboard->GetData(P_ITEMS_IN_FOV, items);
		blackboard->GetData(P_INTERFACE, examInterface);

		auto item = items->front();

		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(item, itemInfo);

		bool hasDestroyed = examInterface->Item_Destroy(item);
		if (!hasDestroyed)
		{
			PrintMessage("Failed to destroy item, probably out of range");
		}

		return BehaviorState::Success;
	}

	BehaviorState SetItemAsTarget(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};

		blackboard->GetData(P_ITEMS_IN_FOV, items);


		const auto item = items->front();
		
		blackboard->ChangeData(P_TARGETINFO, item.Location);
		return BehaviorState::Success;
	}

	BehaviorState SetHouseAsActive(Blackboard* blackboard)
	{
		std::vector<HouseInfo> houses{};
		std::vector<KnownHouse> knownHouses{};
		AgentInfo agentInfo{};

		blackboard->GetData(P_HOUSES_IN_FOV, houses);
		blackboard->GetData(P_PLAYERINFO, agentInfo);
		blackboard->GetData(P_KNOWN_HOUSES, knownHouses);

		bool housesFound{ houses.size() > 0 };
		bool shouldCheckoutHouse{};

		auto newHouse = houses[0];

		auto foundIt = std::find_if(knownHouses.begin(), knownHouses.end(), [newHouse](KnownHouse house) {
			float distance = Elite::Distance(house.housePosition, newHouse.Center);
			return distance <= FLT_EPSILON;
			});

		// Found
		if (foundIt != knownHouses.end())
		{
			KnownHouse house = *foundIt;

			if (house.lastSweepTime >= CONFIG_SWEEP_MAX_TIMEOUT)
			{
				shouldCheckoutHouse = true;
			}
		}
		else
		{
			shouldCheckoutHouse = true;
		}

		if (shouldCheckoutHouse)
		{
			// stop spinning like a silly geese
			blackboard->ChangeData(P_IS_IN_HOUSE, true);

			blackboard->ChangeData(P_TARGETINFO, houses[0].Center);
			blackboard->ChangeData(P_SHOULDEXPLORE, false);

			// Set last position to later exit house
			blackboard->ChangeData(P_LAST_POSITION, agentInfo.Position);
			blackboard->ChangeData(P_ACTIVE_HOUSE, houses[0]);

			// In rare scenarios the turn of the agent is to sharp and it escapes the behaviortree
			blackboard->ChangeData(P_IS_GOING_FOR_HOUSE, true);
		}
		else
		{
			return BehaviorState::Failure;
		}

		return BehaviorState::Success;
	}

	BehaviorState RandomizeVisitLocations(Blackboard* blackboard)
	{
		std::vector<Elite::Vector2> locationsToVisit{};
		std::vector<Elite::Vector2> locationsVisited{};

		blackboard->GetData(P_EXPLORE_LOCATIONS_TO_VISIT, locationsToVisit);
		blackboard->GetData(P_EXPLORE_LOCATIONS_VISITED, locationsVisited);

		for (auto& loc : locationsVisited)
		{
			locationsToVisit.emplace_back(loc);
		}

		// shuffle
		std::random_device rd{};
		std::mt19937 g(rd());
		std::shuffle(locationsToVisit.begin(), locationsToVisit.end(), g);

		// clear visited
		locationsVisited.clear();

		// update blackboard
		blackboard->ChangeData(P_EXPLORE_LOCATIONS_TO_VISIT, locationsToVisit);
		blackboard->ChangeData(P_EXPLORE_LOCATIONS_VISITED, locationsVisited);
		blackboard->ChangeData(P_DESTINATION, locationsToVisit[0]);

		return BehaviorState::Success;
	}

	BehaviorState UpdateExplorationList(Blackboard* blackboard)
	{
		std::vector<Elite::Vector2> locationsToVisit{};
		std::vector<Elite::Vector2> locationsVisited{};
		AgentInfo playerInfo{};
	
		blackboard->GetData(P_EXPLORE_LOCATIONS_TO_VISIT, locationsToVisit);
		blackboard->GetData(P_EXPLORE_LOCATIONS_VISITED, locationsVisited);
		blackboard->GetData(P_PLAYERINFO, playerInfo);

		// Get reached element location
		auto closestLocIt = std::find_if(locationsToVisit.begin(), locationsToVisit.end(), [playerInfo](Elite::Vector2 loc) {
			return Elite::DistanceSquared(playerInfo.Position, loc) <= CONFIG_HAS_REACHED_DESTINATION * CONFIG_HAS_REACHED_DESTINATION;
		});

		// catch if location wouldn't exist == should not be possible
		if (closestLocIt == locationsToVisit.end())
		{
			throw std::runtime_error("Closest location not found");
		}

		// place in visited
		locationsVisited.emplace_back(*closestLocIt);

		// remove from todo
		auto toRemove = std::remove(locationsToVisit.begin(), locationsToVisit.end(), *closestLocIt);
		locationsToVisit.erase(toRemove);

		// Update blackboard
		blackboard->ChangeData(P_EXPLORE_LOCATIONS_TO_VISIT, locationsToVisit);
		blackboard->ChangeData(P_EXPLORE_LOCATIONS_VISITED, locationsVisited);

		return BehaviorState::Success;
	}

	BehaviorState SetNewExploreDestination(Blackboard* blackboard)
	{
		std::vector<Elite::Vector2> locationsToVisit{};

		blackboard->GetData(P_EXPLORE_LOCATIONS_TO_VISIT, locationsToVisit);

		auto newLocation = locationsToVisit[locationsToVisit.size() - 1];

		blackboard->ChangeData(P_DESTINATION, newLocation);

		return BehaviorState::Success;
	}

	BehaviorState AddHouseToVisited(Blackboard* blackboard)
	{
		std::vector<KnownHouse> knownHouses{};
		HouseInfo activeHouse{};
	
		blackboard->GetData(P_ACTIVE_HOUSE, activeHouse);
		blackboard->GetData(P_KNOWN_HOUSES, knownHouses);

		knownHouses.push_back(KnownHouse{ activeHouse.Center, 0.f });

		// update blackboard
		blackboard->ChangeData(P_KNOWN_HOUSES, knownHouses);

		return BehaviorState::Success;
	}

	BehaviorState Face(Blackboard* blackboard)
	{
		Elite::Vector2 target{};
		AgentInfo playerInfo{};

		blackboard->GetData(P_TARGETINFO, target);
		blackboard->GetData(P_PLAYERINFO, playerInfo);

		SteeringPlugin_Output steering{};

		auto targetDir = target - playerInfo.Position;
		auto angleTo = atan2f(targetDir.y, targetDir.x) + float(E_PI_2);
		auto angleFrom = atan2f(sinf(playerInfo.Orientation), cosf(playerInfo.Orientation));
		auto deltaAngle = angleTo - angleFrom;

		auto playerNormal = Elite::Vector2(cos(playerInfo.Orientation), sin(playerInfo.Orientation));
		auto targetNormal = targetDir.GetNormalized();
		auto direction = Elite::Cross(targetNormal, playerNormal);

		if (direction < 0)
		{
			direction = 1.f;
		}
		else
		{
			direction = -1.f;
		}

		steering.LinearVelocity = Elite::Vector2{0.f, 0.f};
		steering.AngularVelocity = deltaAngle * CONFIG_TURN_SPEED * direction;
		steering.AutoOrient = false;

		blackboard->ChangeData(P_STEERING, steering);

		return BehaviorState::Success;
	}

	BehaviorState SetAsTarget(Blackboard* blackboard)
	{
		std::vector<EnemyInfo>* enemies{};

		blackboard->GetData(P_ENEMIES_IN_FOV, enemies);

		if (enemies->size() <= 0)
		{
			return BehaviorState::Failure;
		}

		blackboard->ChangeData(P_TARGETINFO, enemies->front().Location);
		return BehaviorState::Success;
	}

	BehaviorState SetRunAsTarget(Blackboard* blackboard)
	{
		Elite::Vector2 destination{};

		blackboard->GetData(P_DESTINATION, destination);
		blackboard->ChangeData(P_TARGETINFO, destination);

		return BehaviorState::Success;
	}

	BehaviorState DestroyGarbage(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};
		blackboard->GetData(P_ITEMS_IN_FOV, items);
		blackboard->GetData(P_INTERFACE, examInterface);

		// get first item
		auto item = items->front();

		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(item, itemInfo);

		if (!isItem)
		{
			return BehaviorState::Failure;
		}

		bool hasDestroyed = examInterface->Item_Destroy(item);
		if (!hasDestroyed)
		{
			PrintMessage("Failed to destroy garbage, probably out of range");
		}

		return BehaviorState::Success;
	}

	BehaviorState PickupItem(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		Inventory inventory{};
		IExamInterface* examInterface{};

		blackboard->GetData(P_ITEMS_IN_FOV, items);
		blackboard->GetData(P_INVENTORY, inventory);
		blackboard->GetData(P_INTERFACE, examInterface);

		// get first item
		auto item = items->front();

		// get item info
		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(item, itemInfo);

		// early exit if not item OR garbage
		if (!isItem || itemInfo.Type == eItemType::GARBAGE)
		{
			return BehaviorState::Failure;
		}
		
		const auto canGrab = examInterface->Item_Grab(item, itemInfo);
		if (!canGrab)
		{
			return BehaviorState::Failure;
			PrintMessage("Not close enough to grab item, consider placing this after inRange");
		}

		// Add item
		UINT slot = inventory.HasEmptySlot();
		examInterface->Inventory_AddItem(slot, itemInfo);

		// Add item and occupy slot
		inventory.FillSlot(slot, itemInfo);
		blackboard->ChangeData(P_INVENTORY, inventory);

		return BehaviorState::Success;
	}

	BehaviorState Pickup(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};
		AgentInfo agentInfo{};
		Inventory inventory{};
		Elite::Vector2 targetInfo{};

		blackboard->GetData(P_ITEMS_IN_FOV, items);
		blackboard->GetData(P_INTERFACE, examInterface);
		blackboard->GetData(P_INVENTORY, inventory);
		blackboard->GetData(P_PLAYERINFO, agentInfo);
		blackboard->GetData(P_TARGETINFO, targetInfo);

		// Get empty slot + validate if valid slot
		UINT slot = inventory.HasEmptySlot();
		if (slot == inventory.slots.size())
		{
			throw std::runtime_error("Error, pickup should only be called when slot is available");
			return BehaviorState::Failure;
		}

		// Get item
		EntityInfo item = items->front();

		// Check if item and fill in details
		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(item, itemInfo);

		if (!isItem)
		{
			return BehaviorState::Failure;
		}
			
		// Garbage is handled by different stage
		if (itemInfo.Type != eItemType::GARBAGE)
		{
			// Determine if its worth to pick up item
			if (!inventory.ShouldPickupItem(examInterface, itemInfo) && itemInfo.Type != eItemType::FOOD)
			{
				return BehaviorState::Running;
			}
			else
			{
				// Delete less amount item
				UINT slot = inventory.GetSameTypeItemSlot(itemInfo.Type);

				// If slot is valid
				if (slot != inventory.slots.size())
				{
					if (itemInfo.Type == eItemType::FOOD)
					{
						inventory.RemoveSlot(slot);
						examInterface->Inventory_UseItem(slot);
						blackboard->ChangeData(P_INVENTORY, inventory);
						PrintMessage("Consumed leftover food to pickup more");
					}
					else
					{
						// Remove lower grade item
						inventory.RemoveSlot(slot);
						examInterface->Inventory_RemoveItem(slot);
						blackboard->ChangeData(P_INVENTORY, inventory);
					}
				}
			}

			// Grab item, if within range
			const auto maxGrabRange = agentInfo.GrabRange;
			const auto grabRange = Elite::DistanceSquared(item.Location, agentInfo.Position);

			if (grabRange < maxGrabRange * maxGrabRange)
			{
				if (const auto canGrab = examInterface->Item_Grab(item, itemInfo))
				{
					if (!canGrab)
					{
						std::cout << "error grabbing item" << std::endl;
					}
				}

				// Add item
				examInterface->Inventory_AddItem(slot, itemInfo);

				// Add item and occupy slot
				inventory.FillSlot(slot, itemInfo);
				blackboard->ChangeData(P_INVENTORY, inventory);

				return BehaviorState::Success;
			}
			else
			{
				// Not close so set location and let seek handle movement
				targetInfo = item.Location;
				blackboard->ChangeData(P_TARGETINFO, targetInfo);
				return BehaviorState::Running;
			}
		}
		// Garbage
		else
		{
			// Grab item, if within range
			const auto maxGrabRange = agentInfo.GrabRange;
			const auto grabRange = Elite::DistanceSquared(itemInfo.Location, agentInfo.Position);
			if (grabRange < maxGrabRange * maxGrabRange && examInterface->Item_Grab(item, itemInfo))
			{
				// Add item
				examInterface->Inventory_AddItem(slot, itemInfo);

				// Add item and occupy slot
				inventory.FillSlot(slot, itemInfo);
				blackboard->ChangeData(P_INVENTORY, inventory);
			}

			return BehaviorState::Success;
		}

		blackboard->ChangeData(P_TARGETINFO, targetInfo);
		return BehaviorState::Success;
	}

	BehaviorState Drop(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};
		AgentInfo agentInfo{};
		Inventory inventory{};
		Elite::Vector2 targetInfo{};

		bool dataFound =
			blackboard->GetData(P_ITEMS_IN_FOV, items) &&
			blackboard->GetData(P_INTERFACE, examInterface) &&
			blackboard->GetData(P_INVENTORY, inventory) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo) &&
			blackboard->GetData(P_TARGETINFO, targetInfo);

		if (!dataFound)
		{
			return BehaviorState::Failure;
		}

		// Remove any garbage collected
		for (size_t i{}; i < 5; i++)
		{
			if (inventory.items[i].Type == eItemType::GARBAGE)
			{
				inventory.RemoveSlot(i);
				examInterface->Inventory_RemoveItem(i);

				blackboard->ChangeData(P_INVENTORY, inventory);
			}
		}

		for (auto& entityInfo : *items)
		{
			ItemInfo itemInfo{};
			bool isItem = examInterface->Item_GetInfo(entityInfo, itemInfo);
			if (!isItem)
				continue;

			UINT slot = inventory.DetermineUselessItemSlot(examInterface, itemInfo);

			if (slot != inventory.slots.size())
			{
				// remove data
				inventory.RemoveSlot(slot);
				examInterface->Inventory_RemoveItem(slot);
			}
			else
			{
				return BehaviorState::Failure;
			}
		}

		return BehaviorState::Success;
	}

	BehaviorState Seek(Blackboard* blackboard)
	{
		Elite::Vector2 targetPos{};
		AgentInfo agentInfo{};
		IExamInterface* pluginInterface{};
		SteeringPlugin_Output output{};
		bool isInsideHouse{};

		bool dataFound = blackboard->GetData(P_TARGETINFO, targetPos) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo) &&
			blackboard->GetData(P_INTERFACE, pluginInterface) &&
			blackboard->GetData(P_IS_IN_HOUSE, isInsideHouse);

		if (!dataFound)
		{
			return BehaviorState::Failure;
		}

		targetPos = pluginInterface->NavMesh_GetClosestPathPoint(targetPos);
		pluginInterface->Draw_Point(targetPos, 5.f, Elite::Vector3{ 0,1,0 });

		output.RunMode = false;

		if (isInsideHouse)
		{
			output.AutoOrient = true;
			output.AngularVelocity = 0;
		}
		else
		{
			output.AutoOrient = false;
			output.AngularVelocity = CONFIG_TURN_SPEED;
		}
		
		output.LinearVelocity = targetPos - agentInfo.Position;
		output.LinearVelocity.Normalize();
		output.LinearVelocity *= agentInfo.MaxLinearSpeed;

		blackboard->ChangeData(P_STEERING, output);
		return BehaviorState::Success;
	}

	BehaviorState Explore(Blackboard* blackboard)
	{

		Elite::Vector2 destination{};
		

		bool dataFound = blackboard->GetData(P_DESTINATION, destination);
		if (!dataFound)
		{
			return BehaviorState::Failure;
		}

		blackboard->ChangeData(P_TARGETINFO, destination);

		return BehaviorState::Success;
	}

	BehaviorState Sweep(Blackboard* blackboard)
	{
		Elite::Vector2 targetPos{};
		AgentInfo agentInfo{};
		IExamInterface* pluginInterface{};
		SteeringPlugin_Output output{};
		SweepHouse sweepHouse{};

		bool dataFound = blackboard->GetData(P_TARGETINFO, targetPos) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo) &&
			blackboard->GetData(P_INTERFACE, pluginInterface) &&
			blackboard->GetData(P_STEERING, output) &&
			blackboard->GetData(P_HOUSE_TO_SWEEP, sweepHouse);

		if (!dataFound)
		{
			return BehaviorState::Failure;
		}

		bool hasCompletedSweep = sweepHouse.Sweep(agentInfo.Position);

		// Head towards the house as long as not close to center
		if (!hasCompletedSweep)
		{
			targetPos = pluginInterface->NavMesh_GetClosestPathPoint(sweepHouse.GetNextSweepSpot());

			output.AutoOrient = true;
			output.LinearVelocity = targetPos - agentInfo.Position;
			output.LinearVelocity.Normalize();
			output.LinearVelocity *= agentInfo.MaxLinearSpeed;

			blackboard->ChangeData(P_HOUSE_TO_SWEEP, sweepHouse);
		}
		else
		{
			HouseInfo activeHouse{};
			std::vector<KnownHouse> knownHouses{};

			blackboard->GetData(P_ACTIVE_HOUSE, activeHouse);
			blackboard->GetData(P_KNOWN_HOUSES, knownHouses);

			bool houseExists{};

			for (KnownHouse& house : knownHouses)
			{
				if (Elite::Distance(house.housePosition, activeHouse.Center) <= FLT_EPSILON)
				{
					// Reference will update original list
					house.lastSweepTime = 0.f;
				}
			}

			if (!houseExists)
			{
				knownHouses.push_back(KnownHouse{ activeHouse.Center, 0.f });
			}

			blackboard->ChangeData(P_KNOWN_HOUSES, knownHouses);
		}

		blackboard->ChangeData(P_STEERING, output);
	}

	BehaviorState ExitHouse(Blackboard* blackboard)
	{
		Elite::Vector2 targetPos{};
		AgentInfo agentInfo{};
		IExamInterface* pluginInterface{};
		SteeringPlugin_Output output{};

		bool dataFound = blackboard->GetData(P_DESTINATION, targetPos) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo) &&
			blackboard->GetData(P_INTERFACE, pluginInterface);

		if (!dataFound)
		{
			return BehaviorState::Failure;
		}

		targetPos = pluginInterface->NavMesh_GetClosestPathPoint(targetPos);

		output.RunMode = false;
		output.LinearVelocity = targetPos - agentInfo.Position;
		output.LinearVelocity.Normalize();
		output.LinearVelocity *= agentInfo.MaxLinearSpeed;

		blackboard->ChangeData(P_STEERING, output);
		blackboard->ChangeData(P_SHOULDEXPLORE, true);
		blackboard->ChangeData(P_IS_GOING_FOR_HOUSE, false);
		blackboard->ChangeData(P_IS_IN_HOUSE, false);

		return BehaviorState::Success;
	}

	BehaviorState Heal(Blackboard* blackboard) 
	{
		Inventory inventory{};
		AgentInfo playerInfo{};
		IExamInterface* examInterface{};

		bool hasData = blackboard->GetData(P_PLAYERINFO, playerInfo)
			&& blackboard->GetData(P_INVENTORY, inventory)
			&& blackboard->GetData(P_INTERFACE, examInterface);

		// no need to check validation (CanHeal did that already)
		auto foundIt = inventory.HasTypeOfInInventory(eItemType::MEDKIT);
		UINT slot = foundIt - inventory.items.begin();

		// Write data to interface and blackboard
		inventory.RemoveSlot(slot);
		examInterface->Inventory_UseItem(slot);
		examInterface->Inventory_RemoveItem(slot);
		blackboard->ChangeData(P_INVENTORY, inventory);

		return BehaviorState::Success;
	}

	BehaviorState Eat(Blackboard* blackboard)
	{
		Inventory inventory{};
		AgentInfo playerInfo{};
		IExamInterface* examInterface{};

		bool hasData = blackboard->GetData(P_PLAYERINFO, playerInfo)
			&& blackboard->GetData(P_INVENTORY, inventory)
			&& blackboard->GetData(P_INTERFACE, examInterface);

		// no need to check validation (CanHeal did that already)
		auto foundIt = inventory.HasTypeOfInInventory(eItemType::FOOD);
		UINT slot = foundIt - inventory.items.begin();

		// Write data to interface and blackboard
		inventory.RemoveSlot(slot);
		examInterface->Inventory_UseItem(slot);
		examInterface->Inventory_RemoveItem(slot);
		blackboard->ChangeData(P_INVENTORY, inventory);

		return BehaviorState::Success;
	}

	BehaviorState FaceZombie(Blackboard* blackboard)
	{
		AgentInfo playerInfo{};
		IExamInterface* examInterface{};
		Elite::Vector2 zombie{};
		SteeringPlugin_Output output{};

		bool hasData = blackboard->GetData(P_ZOMBIE_TARGET, zombie) &&
			blackboard->GetData(P_PLAYERINFO, playerInfo) &&
			blackboard->GetData(P_INTERFACE, examInterface) &&
			blackboard->GetData(P_STEERING, output);

		auto targetDir = zombie - playerInfo.Position;
		auto angleTo = atan2f(targetDir.y, targetDir.x) + float(E_PI_2);
		auto angleFrom = atan2f(sinf(playerInfo.Orientation), cosf(playerInfo.Orientation));
		auto deltaAngle = angleTo - angleFrom;

		output.AngularVelocity = deltaAngle * CONFIG_TURN_SPEED;
		output.AutoOrient = false;

		blackboard->ChangeData(P_STEERING, output);

		if (deltaAngle >= 1.f)
		{
			return BehaviorState::Running;
		}
		else 
		{
			return BehaviorState::Success;
		}
	}

	BehaviorState Shoot(Blackboard* blackboard)
	{
		Inventory inventory{};
		IExamInterface* examInterface{};

		blackboard->GetData(P_INVENTORY, inventory);
		blackboard->GetData(P_INTERFACE, examInterface);

		auto pistolIt = inventory.HasTypeOfInInventory(eItemType::PISTOL);
		auto shotgunIt = inventory.HasTypeOfInInventory(eItemType::SHOTGUN);

		if (pistolIt != inventory.items.end())
		{
			UINT slot = pistolIt - inventory.items.begin();

			auto oldAmmoCount = examInterface->Weapon_GetAmmo((*pistolIt));
			examInterface->Inventory_UseItem(slot);

			if (oldAmmoCount - 1 <= 0)
			{
				inventory.RemoveSlot(slot);
				blackboard->ChangeData(P_INVENTORY, inventory);
				examInterface->Inventory_RemoveItem(slot);
			}
		}
		else if (shotgunIt != inventory.items.end())
		{
			UINT slot = shotgunIt - inventory.items.begin();

			auto oldAmmoCount = examInterface->Weapon_GetAmmo((*pistolIt));
			examInterface->Inventory_UseItem(slot);

			if (oldAmmoCount - 1 <= 0)
			{
				inventory.RemoveSlot(slot);
				blackboard->ChangeData(P_INVENTORY, inventory);
				examInterface->Inventory_RemoveItem(slot);
			}
		}

		return BehaviorState::Success;
	}

	BehaviorState Turn(Blackboard* blackboard)
	{
		SteeringPlugin_Output output{};

		output.AutoOrient = false;
		output.AngularVelocity = CONFIG_TURN_SPEED;

		blackboard->ChangeData(P_STEERING, output);
		return BehaviorState::Success;
	}

	BehaviorState RunForestRun(Blackboard* blackboard) 
	{
		Elite::Vector2 targetPos{};
		AgentInfo playerInfo{};
		IExamInterface* examInterface{};
		SteeringPlugin_Output output{};
		bool canRun{};

		bool dataFound = blackboard->GetData(P_TARGETINFO, targetPos) &&
			blackboard->GetData(P_PLAYERINFO, playerInfo) &&
			blackboard->GetData(P_INTERFACE, examInterface);

		targetPos = examInterface->NavMesh_GetClosestPathPoint(targetPos);

		output.RunMode = true;
		output.AutoOrient = true;
		output.LinearVelocity = targetPos - playerInfo.Position;
		output.LinearVelocity.Normalize();
		output.LinearVelocity *= playerInfo.MaxLinearSpeed;

		blackboard->ChangeData(P_STEERING, output);
		return BehaviorState::Success;
	}
}

namespace BT_Conditions
{

	bool ShouldExplore(Blackboard* blackboard)
	{
		bool shouldExplore{};

		bool dataFound = blackboard->GetData(P_SHOULDEXPLORE, shouldExplore);

		if (!dataFound)
		{
			return false;
		}

		return shouldExplore;
	}
	
	bool IsHouseInFOV(Blackboard* blackboard)
	{
		std::vector<HouseInfo> houses{};

		bool dataFound = blackboard->GetData(P_HOUSES_IN_FOV, houses);

		if (!dataFound)
		{
			return false;
		}

		return houses.size() > 0;
	}

	bool IsInHouse(Blackboard* blackboard)
	{
		AgentInfo agentInfo{};
		HouseInfo houseInfo{};

		bool dataFound = blackboard->GetData(P_ACTIVE_HOUSE, houseInfo) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo);

		if (!dataFound)
		{
			return false;
		}
		
		// In house
		bool isInHouse (
			agentInfo.Position.x > houseInfo.Center.x - houseInfo.Size.x / 2 &&
			agentInfo.Position.x < houseInfo.Center.x + houseInfo.Size.x / 2 &&
			agentInfo.Position.y > houseInfo.Center.y - houseInfo.Size.y / 2 &&
			agentInfo.Position.y < houseInfo.Center.y + houseInfo.Size.y / 2
		);

		return isInHouse;
	}
	
	bool ShouldSweepHouse(Blackboard* blackboard)
	{
		std::vector<KnownHouse> knownHouses{};
		HouseInfo activeHouse{};
		SweepHouse sweepHouse{};

		bool dataFound =
			blackboard->GetData(P_KNOWN_HOUSES, knownHouses) &&
			blackboard->GetData(P_ACTIVE_HOUSE, activeHouse) &&
			blackboard->GetData(P_HOUSE_TO_SWEEP, sweepHouse);

		if (!dataFound)
		{
			return false;
		}

		auto foundIt = std::find_if(knownHouses.begin(), knownHouses.end(), [activeHouse](KnownHouse house) {
			float distance = Elite::Distance(house.housePosition, activeHouse.Center);
			return distance <= FLT_EPSILON;
		});

		// When sweeping we know we are inside, so we set data
		blackboard->ChangeData(P_IS_IN_HOUSE, true);

		// Found
		if (foundIt != knownHouses.end())
		{
			KnownHouse house = *foundIt;

			if (house.lastSweepTime >= CONFIG_SWEEP_MAX_TIMEOUT)
			{
				if (!sweepHouse.HasGeneratedLocations(activeHouse.Center))
				{
					sweepHouse.GenerateSweepLocations(activeHouse);
					blackboard->ChangeData(P_HOUSE_TO_SWEEP, sweepHouse);
				}

				return true;
			}
		}
		else
		{
			if (!sweepHouse.HasGeneratedLocations(activeHouse.Center))
			{
				sweepHouse.GenerateSweepLocations(activeHouse);
				blackboard->ChangeData(P_HOUSE_TO_SWEEP, sweepHouse);
			}

			return true;
		}

		return false;
	}

	bool IsGoingToHouse(Blackboard* blackboard)
	{
		bool isGoingForHouse{};
		HouseInfo houseInfo{};
		Elite::Vector2 target{};

		bool dataFound = blackboard->GetData(P_IS_GOING_FOR_HOUSE, isGoingForHouse) &&
			blackboard->GetData(P_ACTIVE_HOUSE, houseInfo) &&
			blackboard->GetData(P_TARGETINFO, target);

		if (!dataFound)
		{
			return false;
		}

		// Sometimes a target can be overwritten, usually in debug mode only, this is a unstuck mechanism.
		if (Elite::Distance(target, houseInfo.Center) > FLT_EPSILON)
		{
			// Reset the target to the house to stop bug
			blackboard->ChangeData(P_TARGETINFO, houseInfo.Center);

			return false;
		}

		return isGoingForHouse;
	}

	bool SeesItem(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};

		bool dataFound =
			blackboard->GetData(P_ITEMS_IN_FOV, items) &&
			blackboard->GetData(P_INTERFACE, examInterface);

		if (!dataFound || items->size() == 0)
		{
			return false;
		}

		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(items->front(), itemInfo);

		// NOT AN ITEM
		if (!isItem)
		{
			return false;
		}

		blackboard->ChangeData(P_IS_IN_HOUSE, true);

		return items->size() != 0;
	}

	bool SeesGarbage(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};

		bool dataFound =
			blackboard->GetData(P_ITEMS_IN_FOV, items) &&
			blackboard->GetData(P_INTERFACE, examInterface);

		if (!dataFound || items->size() == 0)
		{
			return false;
		}

		for (auto entityInfo : *items)
		{
			ItemInfo itemInfo{};
			examInterface->Item_GetInfo(entityInfo, itemInfo);

			if (itemInfo.Type == eItemType::GARBAGE)
			{
				blackboard->ChangeData(P_TARGETINFO, itemInfo.Location);

				return true;
			}
		}

		return false;
	}

	bool HasInventorySlot(Blackboard* blackboard)
	{
		Inventory inventory{};

		bool dataFound =
			blackboard->GetData(P_INVENTORY, inventory);

		if (!dataFound)
		{
			return false;
		}

		// See if has empty slot
		bool hasEmptySlot = inventory.HasEmptySlot() != inventory.slots.size();
		return hasEmptySlot;
	}

	bool IsPlayerLowHealth(Blackboard* blackboard)
	{
		AgentInfo playerInfo{};

		bool hasData = blackboard->GetData(P_PLAYERINFO, playerInfo);

		if (!hasData)
		{
			return false;
		}

		return playerInfo.Health <= CONFIG_MIN_ALLOWED_HEALTH;
	}

	bool CanPlayerHeal(Blackboard* blackboard)
	{
		AgentInfo playerInfo{};
		Inventory inventory{};

		bool hasData = blackboard->GetData(P_PLAYERINFO, playerInfo)
			&& blackboard->GetData(P_INVENTORY, inventory);

		if (!hasData)
		{
			return false;
		}

		return inventory.HasTypeOfInInventory(eItemType::MEDKIT) != inventory.items.end();
	}

	bool IsPlayerLowStamina(Blackboard* blackboard)
	{
		AgentInfo playerInfo{};

		bool hasData = blackboard->GetData(P_PLAYERINFO, playerInfo);

		if (!hasData)
		{
			return false;
		}

		return playerInfo.Energy <= CONFIG_MIN_ALLOWED_STAMINA;
	}

	bool CanPlayerEat(Blackboard* blackboard)
	{
		AgentInfo playerInfo{};
		Inventory inventory{};

		bool hasData = blackboard->GetData(P_PLAYERINFO, playerInfo)
			&& blackboard->GetData(P_INVENTORY, inventory);

		if (!hasData)
		{
			return false;
		}

		return inventory.HasTypeOfInInventory(eItemType::FOOD) != inventory.items.end();
	}

	bool IsZombieInFOV(Blackboard* blackboard)
	{
		std::vector<EnemyInfo>* enemies{};
		IExamInterface* examInterface{};

		bool hasData = blackboard->GetData(P_ENEMIES_IN_FOV, enemies) &&
			blackboard->GetData(P_INTERFACE, examInterface);

		if (!hasData || enemies->size() == 0)
		{
			return false;
		}

		bool doesFOVContainZombie{ false };

		for (auto enemy : *enemies)
		{
			doesFOVContainZombie = true;
			blackboard->ChangeData(P_ZOMBIE_TARGET, enemy.Location);
		}

		return doesFOVContainZombie;
	}

	bool IsPlayerArmed(Blackboard* blackboard)
	{
		Inventory inventory{};

		bool hasData = blackboard->GetData(P_INVENTORY, inventory);

		if (!hasData)
		{
			return false;
		}

		bool hasPistol = inventory.HasTypeOfInInventory(eItemType::PISTOL) != inventory.items.end();
		bool hasShotgun = inventory.HasTypeOfInInventory(eItemType::SHOTGUN) != inventory.items.end();

		bool isArmed = hasPistol || hasShotgun;

		// Return true if player has shotgun or pistol
		return isArmed;
	}

	bool IsPlayerNOTArmed(Blackboard* blackboard)
	{
		Inventory inventory{};

		bool hasData = blackboard->GetData(P_INVENTORY, inventory);

		if (!hasData)
		{
			return false;
		}

		bool hasPistol = inventory.HasTypeOfInInventory(eItemType::PISTOL) != inventory.items.end();
		bool hasShotgun = inventory.HasTypeOfInInventory(eItemType::SHOTGUN) != inventory.items.end();

		bool isArmed = hasPistol || hasShotgun;

		// Return true if player has shotgun or pistol
		return !isArmed;
	}

	bool IsPlayerBitten(Blackboard* blackboard)
	{
		AgentInfo playerInfo{};
		bool playerWasBitten{};

		bool hasData = blackboard->GetData(P_PLAYERINFO, playerInfo) &&
			blackboard->GetData(P_PLAYER_WAS_BITTEN, playerWasBitten);

		if (!hasData)
		{
			return false;
		}

		if (playerWasBitten)
		{
			return true;
		}

		if (playerInfo.WasBitten)
		{
			blackboard->ChangeData(P_PLAYER_WAS_BITTEN, true);
		}

		return playerInfo.WasBitten;
	}

	bool IsFacingEnemy(Blackboard* blackboard)
	{
		std::vector<EnemyInfo>* enemies{};
		AgentInfo playerInfo{};
		AgentInfo agentInfo{};
		SteeringPlugin_Output lastSteering{};

		blackboard->GetData(P_ENEMIES_IN_FOV, enemies);
		blackboard->GetData(P_PLAYERINFO, agentInfo);
		blackboard->GetData(P_STEERING, lastSteering);

		if (enemies->size() == 0)
		{
			return false;
		}

		EnemyInfo& targetEnemy = enemies->front();

		Elite::Vector2 toTargetNormal = (targetEnemy.Location - agentInfo.Position).GetNormalized();
		Elite::Vector2 heading = Elite::OrientationToVector(agentInfo.Orientation);

		// Get difference
		const float dotResult = heading.Dot(toTargetNormal);

		float accuracyMargin = 0.005f;
		if (DistanceSquared(agentInfo.Position, targetEnemy.Location) > exp2f(agentInfo.FOV_Range / 2))
		{
			accuracyMargin = 0.00001f;
		}

		return Elite::AreEqual(dotResult, 1.0f, accuracyMargin);
	}

	bool IsNotFacingEnemy(Blackboard* blackboard)
	{
		std::vector<EnemyInfo>* enemies{};
		AgentInfo playerInfo{};
		AgentInfo agentInfo{};
		SteeringPlugin_Output lastSteering{};

		blackboard->GetData(P_ENEMIES_IN_FOV, enemies);
		blackboard->GetData(P_PLAYERINFO, agentInfo);
		blackboard->GetData(P_STEERING, lastSteering);

		if (enemies->size() == 0)
		{
			return false;
		}

		EnemyInfo& targetEnemy = enemies->front();

		Elite::Vector2 toTargetNormal = (targetEnemy.Location - agentInfo.Position).GetNormalized();
		Elite::Vector2 heading = Elite::OrientationToVector(agentInfo.Orientation);

		// Get difference
		const float dotResult = heading.Dot(toTargetNormal);

		float accuracyMargin = 0.005f;
		if (DistanceSquared(agentInfo.Position, targetEnemy.Location) > exp2f(agentInfo.FOV_Range / 2))
		{
			accuracyMargin = 0.00001f;
		}
		return !Elite::AreEqual(dotResult, 1.0f, accuracyMargin);
	}

	bool HasReachedExploreLocation(Blackboard* blackboard)
	{
		Elite::Vector2 destination{};
		AgentInfo playerInfo{};

		bool hasData = blackboard->GetData(P_DESTINATION, destination)
			&& blackboard->GetData(P_PLAYERINFO, playerInfo);
		
		if (!hasData)
		{
			return false;
		}

		if (Elite::DistanceSquared(destination, playerInfo.Position) <= CONFIG_HAS_REACHED_DESTINATION * CONFIG_HAS_REACHED_DESTINATION)
		{
			return true;
		}

		return false;
	}

	bool HasVisitedAllLocations(Blackboard* blackboard)
	{
		std::vector<Elite::Vector2> locationsToVisit{};
		std::vector<Elite::Vector2> locationsVisited{};

		bool hasData = blackboard->GetData(P_EXPLORE_LOCATIONS_TO_VISIT, locationsToVisit)
			&& blackboard->GetData(P_EXPLORE_LOCATIONS_VISITED, locationsVisited);

		if (!hasData)
		{
			return false;
		}

		if (locationsToVisit.size() == 0)
		{
			return true;
		}

		return false;
	}

	bool IsPlayerInGrabRange(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};
		AgentInfo playerInfo{};

		bool hasData =
			blackboard->GetData(P_ITEMS_IN_FOV, items) &&
			blackboard->GetData(P_INTERFACE, examInterface) &&
			blackboard->GetData(P_PLAYERINFO, playerInfo);

		if (!hasData)
		{
			return false;
		}


		const auto item = items->front();
		const auto maxGrabRange = playerInfo.GrabRange;
		const auto grabRange = Elite::DistanceSquared(item.Location, playerInfo.Position);

		if (grabRange < maxGrabRange * maxGrabRange)
		{
			return true;
		}

		return false;
	}

	bool IsItemFood(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};

		bool hasData =
			blackboard->GetData(P_ITEMS_IN_FOV, items) &&
			blackboard->GetData(P_INTERFACE, examInterface);

		if (!hasData)
		{
			return false;
		}

		if (items->size() == 0)
		{
			PrintMessage("No items in FOV, have you placed this behind a SeesItem cond?");
			return false;
		}

		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(items->front(), itemInfo);
		if (!isItem)
		{
			PrintMessage("Not an item, have you placed this behind a SeesItem cond?");
			return false;
		}
		
		return itemInfo.Type == eItemType::FOOD;
	}

	bool IsItemMedkit(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};

		bool hasData =
			blackboard->GetData(P_ITEMS_IN_FOV, items) &&
			blackboard->GetData(P_INTERFACE, examInterface);

		if (!hasData)
		{
			return false;
		}

		if (items->size() == 0)
		{
			PrintMessage("No items in FOV, have you placed this behind a SeesItem cond?");
			return false;
		}

		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(items->front(), itemInfo);
		if (!isItem)
		{
			PrintMessage("Not an item, have you placed this behind a SeesItem cond?");
			return false;
		}

		return itemInfo.Type == eItemType::MEDKIT;
	}

	bool IsItemPistol(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};

		bool hasData =
			blackboard->GetData(P_ITEMS_IN_FOV, items) &&
			blackboard->GetData(P_INTERFACE, examInterface);

		if (!hasData)
		{
			return false;
		}

		if (items->size() == 0)
		{
			PrintMessage("No items in FOV, have you placed this behind a SeesItem cond?");
			return false;
		}

		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(items->front(), itemInfo);
		if (!isItem)
		{
			PrintMessage("Not an item, have you placed this behind a SeesItem cond?");
			return false;
		}

		return itemInfo.Type == eItemType::PISTOL;
	}

	bool IsItemShotgun(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		IExamInterface* examInterface{};

		bool hasData =
			blackboard->GetData(P_ITEMS_IN_FOV, items) &&
			blackboard->GetData(P_INTERFACE, examInterface);

		if (!hasData)
		{
			return false;
		}

		if (items->size() == 0)
		{
			PrintMessage("No items in FOV, have you placed this behind a SeesItem cond?");
			return false;
		}

		ItemInfo itemInfo{};
		bool isItem = examInterface->Item_GetInfo(items->front(), itemInfo);
		if (!isItem)
		{
			PrintMessage("Not an item, have you placed this behind a SeesItem cond?");
			return false;
		}

		return itemInfo.Type == eItemType::SHOTGUN;
	}

	bool HasShotgun(Blackboard* blackboard)
	{
		Inventory inventory{};

		bool hasData = blackboard->GetData(P_INVENTORY, inventory);

		if (!hasData)
		{
			return false;
		}

		auto it = inventory.HasTypeOfInInventory(eItemType::SHOTGUN);
		
		return it < inventory.items.end();
	}

	bool HasPistol(Blackboard* blackboard)
	{
		Inventory inventory{};

		bool hasData = blackboard->GetData(P_INVENTORY, inventory);

		if (!hasData)
		{
			return false;
		}

		auto it = inventory.HasTypeOfInInventory(eItemType::PISTOL);

		return it < inventory.items.end();
	}

	bool IsNewGunBetter(Blackboard* blackboard)
	{
		std::vector<EntityInfo>* items{};
		Inventory inventory{};
		IExamInterface* examInterface{};

		bool hasData = blackboard->GetData(P_ITEMS_IN_FOV, items) &&
			blackboard->GetData(P_INTERFACE, examInterface) &&
			blackboard->GetData(P_INVENTORY, inventory);

		if (!hasData)
		{
			return false;
		}

		auto item = items->front();

		ItemInfo itemInfo{};
		auto isItem = examInterface->Item_GetInfo(item, itemInfo);

		if (!isItem)
		{
			throw std::runtime_error("Not an item");
		}

		auto hasIt = inventory.HasTypeOfInInventory(itemInfo.Type);

		if (hasIt == inventory.items.end())
		{
			return false;
		}

		int currentAmmo = examInterface->Weapon_GetAmmo(*hasIt);
		int newAmmo = examInterface->Weapon_GetAmmo(itemInfo);

		return newAmmo > currentAmmo;
	}
}
