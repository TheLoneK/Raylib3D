#include "game/Game.h"
#include "combat/DamageZones.h"
#include "combat/Effects.h"
#include "combat/Projectiles.h"
#include "core/Config.h"
#include "core/Render.h"
#include "ecs/Components.h"
#include "bosses/Bosses.h"
#include "core/MathUtils.h"
#include "enemies/Enemies.h"
#include "enemies/EnemySystems.h"
#include "items/Pickups.h"
#include <algorithm>
#include <string>

namespace
{
    constexpr float EnemyGridCellSize = 2.0f;
}

//----------------------------------------------------------------------------------
// Setup
//----------------------------------------------------------------------------------
Game::Game(unsigned int seed)
{
    m_camera.fovy = Config::CameraFov;
    m_camera.projection = CAMERA_PERSPECTIVE;

    // ECS: components, shared context, then systems. Systems in the same phase run in registration order
    Ecs::RegisterComponents(m_world);
    m_world.set(GameContext{ &m_level, &m_player, &m_flowField, &m_enemyGrid, &m_assets, &m_combatStats, &m_messages, &m_events });

    EnemySystems::Register(m_world, m_drawSystems);
    Bosses::RegisterSystems(m_world, m_drawSystems);
    Pickups::RegisterSystems(m_world, m_drawSystems);
    Interactions::RegisterSystems(m_world, m_drawSystems);
    Projectiles::RegisterSystems(m_world, m_drawSystems);
    Effects::RegisterSystems(m_world, m_drawSystems);
    DamageZones::RegisterSystems(m_world, m_drawSystems);     // Last: its columns are translucent
    Ecs::RegisterCoreSystems(m_world);
    if (Config::EnableFlecsExplorer) Ecs::EnableExplorer(m_world);

    m_playerCombat.Init(m_world);
    m_interactions.Init(m_world);
    m_hud.Init(m_world);
    m_minimap.Init(m_world);

    LoadLevel(seed, false);
}

void Game::GiveStartingLoadout()
{
    // Slot 7 is left empty for a future weapon
    Inventory &inventory = m_player.GetInventory();
    inventory.SetSlot(0, WeaponType::Pistol);
    inventory.SetSlot(1, WeaponType::Shotgun);
    inventory.SetSlot(2, WeaponType::Rifle);
    inventory.SetSlot(3, WeaponType::Plasma);
    inventory.SetSlot(4, WeaponType::Chainsaw);
    inventory.SetSlot(5, WeaponType::Bow);
    inventory.SetActiveSlot(0);
}

void Game::SpawnEnemies(int count)
{
    Enemies::SpawnGroup(m_world, m_level, m_player.GetPosition(), Config::EnemyMinSpawnDistance,
                        count, Config::GunnerFraction, Config::EnemiesHuntOnSpawn);
}

void Game::SpawnLevelFeatures(const LevelFeatures &features)
{
    for (const KeySpawn &key : features.keys) Pickups::Spawn(m_world, key.position, PickupType::Key, WeaponType::None, key.link);
    for (const SwitchSpawn &wallSwitch : features.switches) Interactions::SpawnSwitch(m_world, wallSwitch.position, wallSwitch.normal, wallSwitch.link);
    for (const PickupSpawn &pickup : features.pickups) Pickups::Spawn(m_world, pickup.position, pickup.type, pickup.weapon);
    if (features.hasBoss) Bosses::SpawnRandom(m_world, features.bossPosition);
}

void Game::LoadLevel(unsigned int seed, bool keepLoadout)
{
    m_seed = seed;
    m_levelTransitionTimer = -1.0f;
    m_events = {};
    LevelGenSettings settings = Config::UseLargeLevel ? LevelGenSettings::Large(seed) : LevelGenSettings::Small(seed);
    LevelFeatures features = LevelGenerator::Generate(m_level, settings);
    m_enemyGrid.Configure(static_cast<float>(m_level.GetWidth())*m_level.GetTileSize(),
                          static_cast<float>(m_level.GetDepth())*m_level.GetTileSize(), EnemyGridCellSize);

    Player previous = m_player;
    m_player.Spawn(features.playerStart, features.playerYaw);
    if (keepLoadout && previous.IsAlive())
    {
        m_player.CarryOverFrom(previous);
    }
    else
    {
        m_player.SetGodMode(previous.IsGodMode());     // Debug toggle survives level reloads
        GiveStartingLoadout();
    }
    m_flowField.Build(m_level, m_level.WorldToCell(features.playerStart));
    m_messages.Clear();

    // Remove every gameplay entity from the previous level
    m_world.delete_with<Enemy>();
    m_world.delete_with<Projectile>();
    m_world.delete_with<Tracer>();
    m_world.delete_with<Impact>();
    m_world.delete_with<Pickup>();
    m_world.delete_with<WallSwitch>();
    m_world.delete_with<DamageZone>();
    m_world.delete_with<StuckArrow>();

    SpawnLevelFeatures(features);
    SpawnEnemies(Config::EnemySpawnCount);
    m_player.UpdateCamera(m_camera);
}

void Game::NextLevel()
{
    m_levelNumber++;
    LoadLevel(m_seed + 1, true);
    m_messages.Push(TextFormat("Level %d", m_levelNumber), Color{ 255, 220, 120, 255 }, 4.0f);
}

void Game::SpawnBossNearPlayer()
{
    float yaw = m_player.GetYaw();
    Vector3 spot = Bosses::FindOpenSpotNear(m_level, m_player.GetPosition(), yaw, 14.0f);
    flecs::entity boss = Bosses::SpawnRandom(m_world, spot);
    m_messages.Push(std::string("Spawned the ") + GetEnemyDef(boss.get<Enemy>().type).name, Color{ 170, 70, 255, 255 });
}

void Game::UpdateLevelTransition(float deltaTime)
{
    // Killing a boss starts the countdown to the next level
    if (m_events.bossKilled && (m_levelTransitionTimer < 0.0f)) m_levelTransitionTimer = Config::LevelTransitionTime;
    m_events.bossKilled = false;

    if (m_levelTransitionTimer < 0.0f) return;
    m_levelTransitionTimer -= deltaTime;
    if (m_levelTransitionTimer <= 0.0f) NextLevel();
}

void Game::MakeAllEnemiesHunt()
{
    m_world.each([](EnemyBrain &brain) { brain.huntsPlayer = true; });
    m_messages.Push("Every enemy is hunting you", RED);
}

//----------------------------------------------------------------------------------
// Frame
//----------------------------------------------------------------------------------
void Game::UpdateGlobalInput()
{
    if (IsKeyPressed(KEY_F1)) m_hud.ToggleHelp();
    if (IsKeyPressed(KEY_F2)) m_player.SetGodMode(!m_player.IsGodMode());
    if (IsKeyPressed(KEY_F3)) SpawnEnemies(Config::EnemyWaveSize);
    if (IsKeyPressed(KEY_F4)) MakeAllEnemiesHunt();
    if (IsKeyPressed(KEY_M)) m_minimap.ToggleExpanded();
    if (IsKeyPressed(KEY_F6)) SpawnBossNearPlayer();
    if (IsKeyPressed(KEY_F5)) LoadLevel(m_seed + 1, false);
    if (!m_player.IsAlive() && IsKeyPressed(KEY_ENTER)) LoadLevel(m_seed, false);
}

void Game::Update(float deltaTime)
{
    UpdateGlobalInput();
    Update(deltaTime, PlayerInput::Read());
}

void Game::Update(float deltaTime, const PlayerInput &input)
{
    deltaTime = std::min(deltaTime, Config::MaxFrameTime);

    m_player.Update(input, m_level, deltaTime);
    m_level.UpdateDoors(deltaTime);
    m_interactions.Update(m_level, m_player, m_messages, input);
    m_flowField.Update(m_level, m_player.GetPosition());     // Only rebuilds when the player changes cell or a door opens

    m_combatStats = {};
    m_playerCombat.Update(m_world, m_world.get<GameContext>(), input, deltaTime);
    m_world.progress(deltaTime);                             // Runs every ECS update system

    m_hud.RegisterHits(m_combatStats);
    m_hud.Update(deltaTime);
    m_messages.Update(deltaTime);
    UpdateLevelTransition(deltaTime);
    m_player.UpdateCamera(m_camera);
}

void Game::Draw()
{
    Render::FogSettings fog = Render::DefaultFog();

    BeginDrawing();
        ClearBackground(fog.color);

        Render::SetView(m_camera.position, fog);
        BeginMode3D(m_camera);
            m_level.Draw();
            Ecs::RunSystems(m_drawSystems);
        EndMode3D();

        m_hud.Draw(Hud::FrameInfo{ m_player, m_level, m_minimap, m_messages, m_interactions.GetPrompt(), m_camera, m_seed,
                                   m_levelNumber, m_levelTransitionTimer });
    EndDrawing();
}
