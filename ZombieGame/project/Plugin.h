#pragma once

#include <memory>
#include <array>

#include "IExamPlugin.h"
#include "Exam_HelperStructs.h"

/************************************************************************/
/* String macros														*/
/************************************************************************/
#define P_PLAYERINFO "playerInfo"
#define P_WORLDINFO "worldInfo"
#define P_TARGETINFO "targetInfo"
#define P_INTERFACE "interface"
#define P_SHOULDEXPLORE "shouldExplore" 
#define P_HOUSES_IN_FOV "housesFOV"
#define P_STEERING "steering"
#define P_LAST_POSITION "lastPosition"
#define P_ACTIVE_HOUSE "activeHouse"
#define P_KNOWN_HOUSES "knownHouses"
#define P_DESTINATION_REACHED "destinationReached"
#define P_DESTINATION "destination"
#define P_IS_GOING_FOR_HOUSE "isGoingForHouse"
#define P_INVENTORY "inventory"
#define P_HOUSE_TO_SWEEP "sweepHouse"
#define P_ZOMBIE_TARGET "zombieTarget"
#define P_PLAYER_WAS_BITTEN "playerWasBitten"
#define P_ENEMIES_IN_FOV "enemiesInFOV"
#define P_ITEMS_IN_FOV "itemsInFOV"
#define P_IS_IN_HOUSE "isInHouse"
#define P_EXPLORE_LOCATIONS_TO_VISIT "exploreLocationsToVisit"
#define P_EXPLORE_LOCATIONS_VISITED "exploreLocationsVisited"

#define CONFIG_SWEEP_MAX_TIMEOUT 50
#define CONFIG_WANDER_ANGLE 45
#define CONFIG_RANDOM_LOCATION_COUNT 10
#define CONFIG_MIN_ALLOWED_HEALTH 2.0
#define CONFIG_MIN_ALLOWED_STAMINA 2.0
#define CONFIG_MAX_HOUSE_SWEEP_SPOTS 4
#define CONFIG_HOUSE_WALL_WIDTH 5
#define CONFIG_TURN_SPEED 50
#define CONFIG_BITTEN_REMEMBER_TIME 5
#define CONFIG_HAS_REACHED_DESTINATION 5

class IBaseInterface;
class IExamInterface;
class Blackboard;
class BehaviorTree;

struct KnownHouse
{
	Elite::Vector2 housePosition{};
	float lastSweepTime{};
};

class Plugin :public IExamPlugin
{
public:
	Plugin() {}; 
	virtual ~Plugin() {};

	void Initialize(IBaseInterface* pInterface, PluginInfo& info) override;
	void DllInit() override;
	void DllShutdown() override;

	void InitGameDebugParams(GameDebugParams& params) override;
	void Update(float dt) override;

	SteeringPlugin_Output UpdateSteering(float dt) override;
	void Render(float dt) const override;

private:
	//Interface, used to request data from/perform actions with the AI Framework
	IExamInterface* m_pInterface = nullptr;
	std::vector<HouseInfo> GetHousesInFOV() const;
	std::vector<EntityInfo> GetEntitiesInFOV() const;

	Elite::Vector2 m_Target = {};
	bool m_CanRun = false; //Demo purpose
	bool m_GrabItem = false; //Demo purpose
	bool m_UseItem = false; //Demo purpose
	bool m_RemoveItem = false; //Demo purpose
	float m_AngSpeed = 0.f; //Demo purpose

	/************************************************************************/
	/* Custom properties													 */
	/************************************************************************/
	BehaviorTree* m_pBehaviorTree;
	Blackboard* m_pBlackboard;

	bool m_ShouldExplore{ true };
	float m_BittenTimer{};

	Elite::Vector2 m_LastPosition{};

	// Random
	std::mt19937 m_Rng;
	std::uniform_real_distribution<float> m_LocationPicker;
	std::uniform_real_distribution<float> m_Norm;

	std::vector<HouseInfo> m_HousesInFOV{};
	std::vector<EnemyInfo> m_EnemiesInFOV{};
	std::vector<EntityInfo> m_ItemsInFOV{};

	std::vector<Elite::Vector2> m_RandomLocationsToVisit{};
	std::vector<Elite::Vector2> m_RandomLocationsVisited{};

	/************************************************************************/
	/* Custom functions                                                      */
	/************************************************************************/
	void SweepFullMap();
	void GenerateRandomVisitLocations();
	void SeperateFOVEntities();
	void ManageBittenTimer(float dt);

	UINT m_InventorySlot = 0;
};

//ENTRY
//This is the first function that is called by the host program
//The plugin returned by this function is also the plugin used by the host program
extern "C"
{
	__declspec (dllexport) IPluginBase* Register()
	{
		return new Plugin();
	}
}