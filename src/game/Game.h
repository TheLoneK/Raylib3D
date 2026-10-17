#pragma once

#include "raylib.h"
#include "flecs.h"
#include "combat/PlayerCombat.h"
#include "core/RenderAssets.h"
#include "ecs/Ecs.h"
#include "ecs/GameContext.h"
#include "player/Player.h"
#include "ui/Hud.h"
#include "ui/Minimap.h"
#include "world/FlowField.h"
#include "world/Level.h"
#include "world/SpatialGrid.h"

//----------------------------------------------------------------------------------
// Game: owns every system and wires them together each frame.
//   Non-ECS:  level, player, flow field, UI (few of them, hand written logic)
//   ECS:      enemies, projectiles, effects (many of them, data + systems)
// Create it after InitWindow (it uploads meshes) and destroy it before CloseWindow.
//----------------------------------------------------------------------------------
class Game
{
public:
    explicit Game(unsigned int seed);
    Game(const Game &) = delete;                // Systems hold pointers into this object
    Game &operator=(const Game &) = delete;

    void LoadLevel(unsigned int seed);          // (Re)generate the level and respawn everything
    void Update(float deltaTime);               // Reads input from the keyboard/mouse
    void Update(float deltaTime, const PlayerInput &input);
    void Draw();

    flecs::world &GetWorld() { return m_world; }
    Player &GetPlayer() { return m_player; }
    const Level &GetLevel() const { return m_level; }
    Minimap &GetMinimap() { return m_minimap; }

private:
    void UpdateGlobalInput();
    void GiveStartingLoadout();
    void SpawnEnemies(int count);

    flecs::world m_world;                       // Declared first so it is destroyed last
    Level m_level;
    Player m_player;
    FlowField m_flowField;
    SpatialGrid m_enemyGrid;
    RenderAssets m_assets;
    CombatStats m_combatStats = {};
    PlayerCombat m_playerCombat;
    Hud m_hud;
    Minimap m_minimap;
    SystemList m_drawSystems;
    Camera3D m_camera = {};
    unsigned int m_seed = 0;
};
