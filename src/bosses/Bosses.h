#pragma once

#include "raylib.h"
#include "ecs/Ecs.h"
#include "ecs/Components.h"

class Level;
class Player;

//----------------------------------------------------------------------------------
// Bosses are enemies (Enemy, Health, Collider... so every weapon and the damage
// code treat them like any other enemy) with a Boss component instead of EnemyBrain.
//
//   BossMovement   shared: wake up, hover, keep range, follow the flow field  (OnUpdate)
//   per boss type  attacks + drawing, see SkullLord.cpp and Lich.cpp
//
// To add a boss: add a BossType and an EnemyType, write <Name>.h/.cpp with Spawn and
// RegisterSystems like the existing two, and add it to Spawn/RegisterSystems in Bosses.cpp.
//----------------------------------------------------------------------------------
namespace Bosses
{
    flecs::entity Spawn(flecs::world &world, BossType type, Vector3 position);
    flecs::entity SpawnRandom(flecs::world &world, Vector3 position);
    void RegisterSystems(flecs::world &world, SystemList &drawSystems);

    // Shared helpers for the boss implementations
    flecs::entity CreateBossEntity(flecs::world &world, BossType type, EnemyType enemyType, Vector3 position, float hoverHeight);
    Vector3 GetCenter(const WorldTransform &transform, const Collider &collider);
    bool IsActive(const Boss &boss, const Player &player);      // Engaged and the player is alive
    Vector3 FindOpenSpotNear(const Level &level, Vector3 center, float angle, float distance);  // Falls back closer, then to center
}
