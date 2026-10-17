#pragma once

//----------------------------------------------------------------------------------
// Non-ECS game state that systems need to read or write. Stored in the flecs world
// as a singleton; systems fetch it with Ecs::GetContext(it).
//----------------------------------------------------------------------------------
class Level;
class Player;
class FlowField;
class SpatialGrid;
class RenderAssets;
class MessageFeed;

struct CombatStats             // Reset every frame, read by the HUD (hit markers)
{
    int hits;
    int kills;
};

struct GameContext
{
    Level *level;
    Player *player;
    const FlowField *flowField;
    SpatialGrid *enemyGrid;
    const RenderAssets *assets;
    CombatStats *combatStats;
    MessageFeed *messages;
};
