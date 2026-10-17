#pragma once

#include "raylib.h"
#include "flecs.h"
#include "ecs/GameContext.h"

struct PlayerInput;

//----------------------------------------------------------------------------------
// Player shooting: hitscan rays or projectile spawns for the active weapon
//----------------------------------------------------------------------------------
class PlayerCombat
{
public:
    void Init(flecs::world &world);     // Builds the queries it needs
    void Update(flecs::world &world, const GameContext &context, const PlayerInput &input);

private:
    void FireHitscan(flecs::world &world, const GameContext &context, Vector3 origin, Vector3 direction);
    void FireProjectile(flecs::world &world, const GameContext &context, Vector3 origin, Vector3 direction);

    flecs::query<> m_livingEnemies;     // Enemy, WorldTransform, Collider, without Dead
};
