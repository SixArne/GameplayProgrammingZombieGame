#include "EliteMath/EMath.h"
#include "EBehaviorTree.h"

#include "Plugin.h"
#include "IExamInterface.h"
#include "Structs.h"

namespace BT_Actions
{
	BehaviorState Pickup(Blackboard* blackboard)
	{
		std::vector<EntityInfo> entities{};
		IExamInterface* examInterface{};
		AgentInfo agentInfo{};
		Inventory inventory{};
		Elite::Vector2 targetInfo{};

		bool dataFound =
			blackboard->GetData(P_ENTITIES_IN_FOV, entities) &&
			blackboard->GetData(P_INTERFACE, examInterface) &&
			blackboard->GetData(P_INVENTORY, inventory) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo) &&
			blackboard->GetData(P_TARGETINFO, targetInfo);

		if (!dataFound)
		{
			return BehaviorState::Failure;
		}

		UINT slot = inventory.HasEmptySlot();

		if (slot == inventory.slots.size())
		{
			return BehaviorState::Failure;
		}

		for (auto& item : entities)
		{
			ItemInfo itemInfo{};
			examInterface->Item_GetInfo(item, itemInfo);

			if (itemInfo.Type != eItemType::GARBAGE)
			{
				const auto maxGrabRange = agentInfo.GrabRange;
				const auto grabRange = Elite::DistanceSquared(itemInfo.Location, agentInfo.Position);

				// Don't pick up massive amounts of food
				if (!inventory.ShouldPickupItem(examInterface, itemInfo))
				{
					return BehaviorState::Failure;
				}
			
				if (grabRange < maxGrabRange && examInterface->Item_Grab({}, itemInfo))
				{
					examInterface->Inventory_AddItem(slot, itemInfo);

					// Add item and occupy slot
					inventory.FillSlot(slot, itemInfo);
					blackboard->ChangeData(P_INVENTORY, inventory);
				
					return BehaviorState::Success;
				}
				else
				{
					targetInfo = itemInfo.Location;
				}
			}
		}

		blackboard->ChangeData(P_TARGETINFO, targetInfo);
		return BehaviorState::Success;
	}

	BehaviorState Drop(Blackboard* blackboard)
	{
		std::vector<EntityInfo> entities{};
		IExamInterface* examInterface{};
		AgentInfo agentInfo{};
		Inventory inventory{};
		Elite::Vector2 targetInfo{};

		bool dataFound =
			blackboard->GetData(P_ENTITIES_IN_FOV, entities) &&
			blackboard->GetData(P_INTERFACE, examInterface) &&
			blackboard->GetData(P_INVENTORY, inventory) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo) &&
			blackboard->GetData(P_TARGETINFO, targetInfo);

		if (!dataFound)
		{
			return BehaviorState::Failure;
		}

		for (auto& item : entities)
		{
			ItemInfo itemInfo{};
			examInterface->Item_GetInfo(item, itemInfo);

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
		bool canRun{};

		bool dataFound = blackboard->GetData(P_TARGETINFO, targetPos) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo) &&
			blackboard->GetData(P_INTERFACE, pluginInterface) &&
			blackboard->GetData(P_CAN_RUN, canRun);

		if (!dataFound)
		{
			return BehaviorState::Failure;
		}

		targetPos = pluginInterface->NavMesh_GetClosestPathPoint(targetPos);

		output.RunMode = canRun;
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
		bool canRun{};

		bool dataFound = blackboard->GetData(P_DESTINATION, targetPos) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo) &&
			blackboard->GetData(P_INTERFACE, pluginInterface) &&
			blackboard->GetData(P_CAN_RUN, canRun);

		if (!dataFound)
		{
			return BehaviorState::Failure;
		}

		targetPos = pluginInterface->NavMesh_GetClosestPathPoint(targetPos);

		output.RunMode = canRun;
		output.LinearVelocity = targetPos - agentInfo.Position;
		output.LinearVelocity.Normalize();
		output.LinearVelocity *= agentInfo.MaxLinearSpeed;

		blackboard->ChangeData(P_STEERING, output);
		blackboard->ChangeData(P_SHOULDEXPLORE, true);
		blackboard->ChangeData(P_IS_GOING_FOR_HOUSE, false);

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
	
	bool IsHouseInPOV(Blackboard* blackboard)
	{
		std::vector<HouseInfo> houses{};
		std::vector<KnownHouse> knownHouses{};
		AgentInfo agentInfo{};

		bool dataFound = blackboard->GetData(P_HOUSES_IN_FOV, houses) &&
			blackboard->GetData(P_PLAYERINFO, agentInfo) &&
			blackboard->GetData(P_KNOWN_HOUSES, knownHouses);

		if (!dataFound)
		{
			return false;
		}


		bool housesFound{ houses.size() > 0 };
		bool shouldCheckoutHouse{};

		if (housesFound)
		{
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
		}

		if (shouldCheckoutHouse)
		{
			blackboard->ChangeData(P_TARGETINFO, houses[0].Center);
			blackboard->ChangeData(P_SHOULDEXPLORE, false);

			// Set last position to later exit house
			blackboard->ChangeData(P_LAST_POSITION, agentInfo.Position);
			blackboard->ChangeData(P_ACTIVE_HOUSE, houses[0]);

			// In rare scenarios the turn of the agent is to sharp and it escapes the behaviortree
			blackboard->ChangeData(P_IS_GOING_FOR_HOUSE, true);
		}

		return shouldCheckoutHouse;
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

		bool dataFound = blackboard->GetData(P_IS_GOING_FOR_HOUSE, isGoingForHouse);

		if (!dataFound)
		{
			return false;
		}

		return isGoingForHouse;
	}

	bool SeesItem(Blackboard* blackboard)
	{
		std::vector<EntityInfo> entities{};
		IExamInterface* examInterface{};

		bool dataFound =
			blackboard->GetData(P_ENTITIES_IN_FOV, entities) &&
			blackboard->GetData(P_INTERFACE, examInterface);

		if (!dataFound || entities.size() == 0)
		{
			return false;
		}

		return entities.size() != 0;
	}

	bool SeesGarbage(Blackboard* blackboard)
	{
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
}

namespace BT_HELPERS 
{
	
}