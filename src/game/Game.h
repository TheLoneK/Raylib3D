#pragma once

#include "raylib.h"
#include "flecs.h"
#include "combat/PlayerCombat.h"
#include "core/MessageFeed.h"
#include "core/RenderAssets.h"
#include "ecs/Ecs.h"
#include "ecs/GameContext.h"
#include "items/Interactions.h"
#include "player/Player.h"
#include "ui/Hud.h"
#include "ui/Minimap.h"
#include "world/FlowField.h"
#include "world/Level.h"
#include "world/LevelGenerator.h"
#include "world/SpatialGrid.h"

//----------------------------------------------------------------------------------
// Game: owns every system and wires them together each frame.
//   Non-ECS:  level, player, flow field, UI (few of them, hand written logic)
//   ECS:      enemies, the boss, projectiles, effects, pickups, switches, damage zones
// Create it after InitWindow (it uploads meshes) and destroy it before CloseWindow.
//----------------------------------------------------------------------------------
class Game
{
public:
    explicit Game(unsigned int seed);
    Game(const Game &) = delete;                // Systems hold pointers into this object
    Game &operator=(const Game &) = delete;

    void LoadLevel(unsigned int seed, bool keepLoadout);    // (Re)generate the level and respawn everything
    void NextLevel();                                       // Next seed, keeps weapons/ammo/health
    void Update(float deltaTime);               // Reads input from the keyboard/mouse
    void Update(float deltaTime, const PlayerInput &input);
    void Draw();

    flecs::world &GetWorld() { return m_world; }
    Player &GetPlayer() { return m_player; }
    const Level &GetLevel() const { return m_level; }
    Level &GetLevel() { return m_level; }
    Minimap &GetMinimap() { return m_minimap; }
    MessageFeed &GetMessages() { return m_messages; }
    float GetLevelTransitionTime() const { return m_levelTransitionTimer; }
    int GetLevelNumber() const { return m_levelNumber; }

private:
    void UpdateGlobalInput();
    void GiveStartingLoadout();
    void SpawnEnemies(int count);
    void SpawnLevelFeatures(const LevelFeatures &features);
    void MakeAllEnemiesHunt();
    void SpawnBossNearPlayer();
    void UpdateLevelTransition(float deltaTime);

    flecs::world m_world;                       // Declared first so it is destroyed last
    Level m_level;
    Player m_player;
    FlowField m_flowField;
    SpatialGrid m_enemyGrid;
    RenderAssets m_assets;
    CombatStats m_combatStats = {};
    GameEvents m_events = {};
    MessageFeed m_messages;
    PlayerCombat m_playerCombat;
    Interactions m_interactions;
    Hud m_hud;
    Minimap m_minimap;
    SystemList m_drawSystems;
    Camera3D m_camera = {};
    unsigned int m_seed = 0;
    int m_levelNumber = 1;
    float m_levelTransitionTimer = -1.0f;      // Counting down to the next level after a boss dies
};
