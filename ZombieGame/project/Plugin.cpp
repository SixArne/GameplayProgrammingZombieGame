#include "stdafx.h"
#include "Plugin.h"
#include "IExamInterface.h"
#include "EBlackboard.h"
#include "EBehaviorTree.h"
#include "Behaviors.h"
#include "Structs.h"

using namespace std;

//Called only once, during initialization
void Plugin::Initialize(IBaseInterface* pInterface, PluginInfo& info)
{
	//Retrieving the interface
	//This interface gives you access to certain actions the AI_Framework can perform for you
	m_pInterface = static_cast<IExamInterface*>(pInterface);

	//Bit information about the plugin
	//Please fill this in!!
	info.BotName = "MinionExam";
	info.Student_FirstName = "Arne";
	info.Student_LastName = "Six";
	info.Student_Class = "2DAE08";

	// Blackboard creation

	m_pBlackboard = new Blackboard();
	m_pBlackboard->AddData(P_PLAYERINFO, m_pInterface->Agent_GetInfo());
	m_pBlackboard->AddData(P_WORLDINFO, m_pInterface->World_GetInfo());
	m_pBlackboard->AddData(P_TARGETINFO, Elite::Vector2{ 0, 0 });
	m_pBlackboard->AddData(P_INTERFACE, m_pInterface);
	m_pBlackboard->AddData(P_SHOULDEXPLORE, m_ShouldExplore);
	m_pBlackboard->AddData(P_HOUSES_IN_FOV, m_HousesInFOV);
	m_pBlackboard->AddData(P_ENEMIES_IN_FOV, &m_EnemiesInFOV);
	m_pBlackboard->AddData(P_ITEMS_IN_FOV, &m_ItemsInFOV);
	m_pBlackboard->AddData(P_STEERING, SteeringPlugin_Output{});
	m_pBlackboard->AddData(P_LAST_POSITION, m_LastPosition);
	m_pBlackboard->AddData(P_ACTIVE_HOUSE, HouseInfo{});
	m_pBlackboard->AddData(P_KNOWN_HOUSES, std::vector<KnownHouse>());
	
	
	m_pBlackboard->AddData(P_IS_GOING_FOR_HOUSE, false);
	m_pBlackboard->AddData(P_INVENTORY, Inventory{});
	m_pBlackboard->AddData(P_HOUSE_TO_SWEEP, SweepHouse{});
	m_pBlackboard->AddData(P_ZOMBIE_TARGET, Elite::Vector2{});
	m_pBlackboard->AddData(P_PLAYER_WAS_BITTEN, false);
	m_pBlackboard->AddData(P_IS_IN_HOUSE, false);

	// Exploration
	m_pBlackboard->AddData(P_EXPLORE_LOCATIONS_TO_VISIT, m_RandomLocationsToVisit);
	m_pBlackboard->AddData(P_EXPLORE_LOCATIONS_VISITED, m_RandomLocationsVisited);
	m_pBlackboard->AddData(P_DESTINATION, Elite::Vector2{});
	m_pBlackboard->AddData(P_DESTINATION_REACHED, false);

	// Tree creation
	m_pBehaviorTree = new BehaviorTree(m_pBlackboard,
		new BehaviorSelector{ {
				/************************************************************************/
				/* Combat                                                               */
				/************************************************************************/
				new BehaviorSelector{{
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsZombieInFOV),
						new BehaviorConditional(BT_Conditions::IsPlayerArmed),
						new BehaviorSelector{{
							new BehaviorSequence{{
								new BehaviorConditional(BT_Conditions::IsFacingEnemy),
								/*new BehaviorAction(BT_Actions::FaceZombie),*/
								new BehaviorAction(BT_Actions::Shoot)
							}},
							new BehaviorSequence{{
								new BehaviorConditional(BT_Conditions::IsNotFacingEnemy),
								new BehaviorAction(BT_Actions::SetAsTarget),
								new BehaviorAction(BT_Actions::Face)
							}},
						}},
					}},
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsZombieInFOV),
						new BehaviorConditional(BT_Conditions::IsInHouse),
						new BehaviorConditional(BT_Conditions::IsPlayerNOTArmed),
						new BehaviorAction(BT_Actions::AddHouseToVisited),
						new BehaviorAction(BT_Actions::SetRunAsTarget),
						new BehaviorAction(BT_Actions::RunForestRun)
					}},
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsPlayerBitten),
						new BehaviorConditional(BT_Conditions::IsPlayerArmed),
						new BehaviorAction(BT_Actions::Turn)
					}},
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsPlayerBitten),
						new BehaviorAction(BT_Actions::RunForestRun)
					}},
				}},
		/************************************************************************/
		/* Item consumption														*/
		/************************************************************************/
		new BehaviorSequence{{
			new BehaviorConditional(BT_Conditions::IsPlayerLowHealth),
			new BehaviorConditional(BT_Conditions::CanPlayerHeal),
			new BehaviorAction(BT_Actions::Heal)
		}},
		new BehaviorSequence{{
			new BehaviorConditional(BT_Conditions::IsPlayerLowStamina),
			new BehaviorConditional(BT_Conditions::CanPlayerEat),
			new BehaviorAction(BT_Actions::Eat)
		}},



		/************************************************************************/
		/* Garbage																*/
		/************************************************************************/
		new BehaviorSelector{{
			new BehaviorSequence{{
				new BehaviorConditional(BT_Conditions::SeesGarbage),
				new BehaviorSelector{{
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsPlayerInGrabRange),
						new BehaviorAction(BT_Actions::DestroyGarbage)
					}},
					new BehaviorSequence{{
						new BehaviorAction(BT_Actions::SetItemAsTarget),
						new BehaviorAction(BT_Actions::Seek),
					}},
				}},
			}},
		}},
		/************************************************************************/
		/* Items																*/
		/************************************************************************/
		new BehaviorSequence{{
			new BehaviorConditional(BT_Conditions::SeesItem),
			new BehaviorSelector{{
					// Consume food before picking up more
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsItemFood),
						new BehaviorConditional(BT_Conditions::IsPlayerInGrabRange),
						new BehaviorConditional(BT_Conditions::CanPlayerEat),
						new BehaviorAction(BT_Actions::Eat)
					}},
					// Consume medkit if hurt and on ground
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsItemMedkit),
						new BehaviorConditional(BT_Conditions::IsPlayerInGrabRange),
						new BehaviorConditional(BT_Conditions::CanPlayerHeal),
						new BehaviorAction(BT_Actions::Heal)
					}},
					// Checks if pistol is worth it 
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsItemPistol),
						new BehaviorConditional(BT_Conditions::IsPlayerInGrabRange),
						new BehaviorSelector{{
							new BehaviorSequence{{
								// If has no pistol pick up
								new BehaviorNotConditional(BT_Conditions::HasPistol),
								new BehaviorAction(BT_Actions::PickupItem),
							}},
							new BehaviorSequence{{
								// If has pistol but new one is better drop old pick up new
								new BehaviorConditional(BT_Conditions::IsNewGunBetter),
								new BehaviorAction(BT_Actions::DropOldGun),
								new BehaviorAction(BT_Actions::PickupItem),
							}},
							new BehaviorSequence{{
								// Destroy less-value gun
								new BehaviorAction(BT_Actions::DestroyGun)
							}}
						}}
					}},
					// Checks if shotgun is worth it
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsItemShotgun),
						new BehaviorConditional(BT_Conditions::IsPlayerInGrabRange),
						new BehaviorSelector{{
							new BehaviorSequence{{
								// If has no pistol pick up
								new BehaviorNotConditional(BT_Conditions::HasShotgun),
								new BehaviorAction(BT_Actions::PickupItem),
							}},
							new BehaviorSequence{{
								// If has pistol but new one is better drop old pick up new
								new BehaviorConditional(BT_Conditions::IsNewGunBetter),
								new BehaviorAction(BT_Actions::DropOldGun),
								new BehaviorAction(BT_Actions::PickupItem),
							}},
							new BehaviorSequence{{
								// Destroy less-value gun
								new BehaviorAction(BT_Actions::DestroyGun)
							}}
						}}
					}},
					// Pick item if has inventory slot and in range
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::IsPlayerInGrabRange),
						new BehaviorConditional(BT_Conditions::HasInventorySlot),
						new BehaviorAction(BT_Actions::PickupItem),
					}},
					// Set seen item as target and seek
					new BehaviorSequence{{
						new BehaviorNotConditional(BT_Conditions::IsPlayerInGrabRange),
						new BehaviorAction(BT_Actions::SetItemAsTarget),
						new BehaviorAction(BT_Actions::Seek)
					}}
				}},
			}},
			
			/************************************************************************/
			/* Sweeping house														*/
			/************************************************************************/
			new BehaviorSequence{{
				new BehaviorConditional(BT_Conditions::IsInHouse),
				new BehaviorSelector{{
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::ShouldSweepHouse),
						new BehaviorAction(BT_Actions::Sweep)
					}},
					new BehaviorSequence{{
						new BehaviorAction(BT_Actions::ExitHouse)
					}},
				}},
			}},
			/************************************************************************/
			/* House detection														*/
			/************************************************************************/
			new BehaviorSelector{{
				new BehaviorSequence{{
					new BehaviorConditional(BT_Conditions::IsGoingToHouse),
					new BehaviorAction(BT_Actions::Seek)
				}},
				new BehaviorSequence{{
					new BehaviorConditional(BT_Conditions::IsHouseInFOV),
					new BehaviorAction(BT_Actions::SetHouseAsActive),
				}},
			}},

			/************************************************************************/
			/* Exploration                                                          */
			/************************************************************************/
			new BehaviorSequence{{
				new BehaviorSelector{{
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::HasVisitedAllLocations),
						new BehaviorAction(BT_Actions::RandomizeVisitLocations)
					}},
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::HasReachedExploreLocation),
						new BehaviorAction(BT_Actions::UpdateExplorationList),
						new BehaviorAction(BT_Actions::SetNewExploreDestination)
					}},
					new BehaviorSequence{{
						new BehaviorConditional(BT_Conditions::ShouldExplore),
						new BehaviorAction(BT_Actions::Explore),
						new BehaviorAction(BT_Actions::Seek)
					}},
				}},
			}},
		}}
	);

	std::random_device rd;
	m_Rng = std::mt19937(rd());

	Elite::Vector2 dim = m_pInterface->World_GetInfo().Dimensions;
	Elite::Vector2 org = m_pInterface->World_GetInfo().Center;


	m_LocationPicker = std::uniform_real_distribution<float>(0, CONFIG_RANDOM_LOCATION_COUNT);
	m_Norm = std::uniform_real_distribution<float>(0.f, 1.f);

	//
	GenerateRandomVisitLocations();
}

void Plugin::DllInit()
{

}

void Plugin::DllShutdown()
{

}

//Called only once, during initialization
void Plugin::InitGameDebugParams(GameDebugParams& params)
{
	params.AutoFollowCam = true; //Automatically follow the AI? (Default = true)
	params.RenderUI = true; //Render the IMGUI Panel? (Default = true)
	params.SpawnEnemies = true; //Do you want to spawn enemies? (Default = true)
	params.EnemyCount = 20; //How many enemies? (Default = 20)
	params.GodMode = false; //GodMode > You can't die, can be useful to inspect certain behaviors (Default = false)
	params.LevelFile = "GameLevel.gppl";
	params.AutoGrabClosestItem = true; //A call to Item_Grab(...) returns the closest item that can be grabbed. (EntityInfo argument is ignored)
	params.StartingDifficultyStage = 1;
	params.InfiniteStamina = false;
	params.SpawnDebugPistol = true;
	params.SpawnDebugShotgun = true;
	params.SpawnPurgeZonesOnMiddleClick = true;
	params.PrintDebugMessages = true;
	params.ShowDebugItemNames = true;
	params.Seed = 600;
}

//Only Active in DEBUG Mode
//(=Use only for Debug Purposes)
void Plugin::Update(float dt)
{
	//Demo Event Code
	//In the end your AI should be able to walk around without external input
	//if (m_pInterface->Input_IsMouseButtonUp(Elite::InputMouseButton::eLeft))
	//{
	//	//Update target based on input
	//	Elite::MouseData mouseData = m_pInterface->Input_GetMouseData(Elite::InputType::eMouseButton, Elite::InputMouseButton::eLeft);
	//	const Elite::Vector2 pos = Elite::Vector2(static_cast<float>(mouseData.X), static_cast<float>(mouseData.Y));
	//	m_Target = m_pInterface->Debug_ConvertScreenToWorld(pos);
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Space))
	//{
	//	m_CanRun = true;
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Left))
	//{
	//	m_AngSpeed -= Elite::ToRadians(10);
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Right))
	//{
	//	m_AngSpeed += Elite::ToRadians(10);
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_G))
	//{
	//	m_GrabItem = true;
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_U))
	//{
	//	m_UseItem = true;
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_R))
	//{
	//	m_RemoveItem = true;
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyUp(Elite::eScancode_Space))
	//{
	//	m_CanRun = false;
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Delete))
	//{
	//	m_pInterface->RequestShutdown();
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_KP_Minus))
	//{
	//	if (m_InventorySlot > 0)
	//		--m_InventorySlot;
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_KP_Plus))
	//{
	//	if (m_InventorySlot < 4)
	//		++m_InventorySlot;
	//}
	//else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Q))
	//{
	//	ItemInfo info = {};
	//	m_pInterface->Inventory_GetItem(m_InventorySlot, info);
	//	std::cout << (int)info.Type << std::endl;
	//}
}

//Update
//This function calculates the new SteeringOutput, called once per frame
SteeringPlugin_Output Plugin::UpdateSteering(float dt)
{
	// Clear data
	m_HousesInFOV.clear();
	m_EnemiesInFOV.clear();
	m_ItemsInFOV.clear();

	// update houses
	m_HousesInFOV = GetHousesInFOV();
	m_pBlackboard->ChangeData(P_HOUSES_IN_FOV, m_HousesInFOV);

	std::vector<HouseInfo> houses{};
	m_pBlackboard->GetData(P_HOUSES_IN_FOV, houses);

	// Set items and enemies in fov
	SeperateFOVEntities();

	// spinning should be false by default
	m_pBlackboard->ChangeData(P_IS_IN_HOUSE, false);

	SteeringPlugin_Output steering{};

	auto agentInfo = m_pInterface->Agent_GetInfo();
	m_pBlackboard->ChangeData(P_PLAYERINFO, agentInfo);

	m_pBehaviorTree->Update(dt);
	m_pBlackboard->GetData(P_STEERING, steering);

	m_GrabItem = false;
	m_UseItem = false;
	m_RemoveItem = false;

	// Update house sweep timer
	std::vector<KnownHouse> knownHouses{};
	m_pBlackboard->GetData(P_KNOWN_HOUSES, knownHouses);

	for (KnownHouse house : knownHouses)
	{
		house.lastSweepTime += dt;
	}

	m_pBlackboard->ChangeData(P_KNOWN_HOUSES, knownHouses);


	//SweepFullMap();
	ManageBittenTimer(dt);
	
	return steering;
}

void Plugin::ManageBittenTimer(float dt)
{
	bool wasBitten{};
	m_pBlackboard->GetData(P_PLAYER_WAS_BITTEN, wasBitten);
	if (wasBitten)
	{
		m_BittenTimer += dt;
	}



	if (m_BittenTimer > CONFIG_BITTEN_REMEMBER_TIME)
	{
		m_pBlackboard->ChangeData(P_PLAYER_WAS_BITTEN, false);
		m_BittenTimer = 0.f;
	}

}

void Plugin::SweepFullMap()
{
	// Exploration section
	auto agentInfo = m_pInterface->Agent_GetInfo();

	Elite::Vector2 destination{};

	bool dataFound =
		m_pBlackboard->GetData(P_DESTINATION, destination);

	if (Elite::Distance(agentInfo.Position, destination) <= 5.f)
	{
		// Pick random location
		m_pBlackboard->ChangeData(P_DESTINATION, m_RandomLocationsToVisit[m_LocationPicker(m_Rng)]);
	}
}

void Plugin::SeperateFOVEntities()
{
	auto entities = GetEntitiesInFOV();

	for (auto& entity : entities)
	{
		switch (entity.Type)
		{
		case eEntityType::ENEMY:
		{
			EnemyInfo enemyInfo{};
			m_pInterface->Enemy_GetInfo(entity, enemyInfo);
			m_EnemiesInFOV.push_back(enemyInfo);
		}
		break;
		case eEntityType::ITEM:
		{
			m_ItemsInFOV.push_back(entity);
		}
		break;
		default:
			break;
		}
	}
}

void Plugin::GenerateRandomVisitLocations()
{

	/************************************************************************/
	/* Algorithm to sweep from inside to outside                            */
	/************************************************************************/
	WorldInfo worldInfo = m_pInterface->World_GetInfo();

	// Important houses are only contained within the first 30%
	float worldMapPercentage = .3f;

	for (uint32_t i{0}; i <= 360; i += 36)
	{
		// Add random points on unit circle to visit.
		float x = cos((float)i) * worldInfo.Dimensions.x * worldMapPercentage;
		float y = sin((float)i) * worldInfo.Dimensions.y * worldMapPercentage;

		m_RandomLocationsToVisit.push_back(Elite::Vector2(x, y));
	}

	// shuffle locations
	std::random_device rd{};
	std::mt19937 g(rd());
	std::shuffle(m_RandomLocationsToVisit.begin(), m_RandomLocationsToVisit.end(), g);

	// Reserve visited list to same count as to visit
	m_RandomLocationsVisited.reserve(m_RandomLocationsToVisit.size());

	// Set blackboard data
	m_pBlackboard->ChangeData(P_DESTINATION, m_RandomLocationsToVisit[0]);
	m_pBlackboard->ChangeData(P_EXPLORE_LOCATIONS_TO_VISIT, m_RandomLocationsToVisit);
	m_pBlackboard->ChangeData(P_EXPLORE_LOCATIONS_VISITED, m_RandomLocationsVisited);
}

//This function should only be used for rendering debug elements
void Plugin::Render(float dt) const
{
	Elite::Vector2 targetPos{};
	Elite::Vector2 destPos{};
	AgentInfo agentInfo{};
	HouseInfo houseInfo{};
	std::vector<Elite::Vector2> locationsToVisit{};
	std::vector<Elite::Vector2> locationsVisited{};

	// explore debug
	m_pBlackboard->GetData(P_EXPLORE_LOCATIONS_TO_VISIT, locationsToVisit);
	m_pBlackboard->GetData(P_EXPLORE_LOCATIONS_VISITED, locationsVisited);
	m_pBlackboard->GetData(P_TARGETINFO, targetPos);
	m_pBlackboard->GetData(P_ACTIVE_HOUSE, houseInfo);
	m_pBlackboard->GetData(P_DESTINATION, destPos);
	m_pBlackboard->GetData(P_PLAYERINFO, agentInfo);

	m_pInterface->Draw_Segment(agentInfo.Position, targetPos, { 0,0,0 });
	m_pInterface->Draw_SolidCircle(houseInfo.Center, .7f, { 0,0 }, { 1, 0, 1 });
	m_pInterface->Draw_SolidCircle(destPos, 2.f, { 0,0 }, { 1, 1, 1 });

	for (auto loc : locationsToVisit)
	{
		m_pInterface->Draw_Segment(loc, {0,0}, { 0,0,0 });
		m_pInterface->Draw_Circle(loc, 10.f, { 1,0,1 });
	}

	SweepHouse sweepHouse{};

	m_pBlackboard->GetData(P_HOUSE_TO_SWEEP, sweepHouse);

	for (auto loc : sweepHouse.sweepLocations)
	{
		m_pInterface->Draw_SolidCircle(loc, .7f, { 0,0 }, { 0, 0, 1 });
	}
}

vector<HouseInfo> Plugin::GetHousesInFOV() const
{
	vector<HouseInfo> vHousesInFOV = {};

	HouseInfo hi = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetHouseByIndex(i, hi))
		{
			vHousesInFOV.push_back(hi);
			continue;
		}

		break;
	}

	return vHousesInFOV;
}

vector<EntityInfo> Plugin::GetEntitiesInFOV() const
{
	vector<EntityInfo> vEntitiesInFOV = {};

	EntityInfo ei = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetEntityByIndex(i, ei))
		{
			vEntitiesInFOV.push_back(ei);
			continue;
		}

		break;
	}

	return vEntitiesInFOV;
}
