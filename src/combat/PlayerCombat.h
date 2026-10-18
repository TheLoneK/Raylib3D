#pragma once

#include "raylib.h"
#include "flecs.h"
#include "ecs/GameContext.h"

struct PlayerInput;
struct WeaponDef;

//----------------------------------------------------------------------------------
// Player shooting: hitscan rays, projectile spawns (including charged bow shots)
// and melee arcs for the active weapon
//----------------------------------------------------------------------------------
class PlayerCombat
{
public:
    void Init(flecs::world &world);     // Builds the queries it needs
    void Update(flecs::world &world, const GameContext &context, const PlayerInput &input, float deltaTime);

    // Damage for one pellet/projectile/tick after every player-side multiplier
    static float GetPlayerDamage(const WeaponDef &def, const GameContext &context);

private:
    void FireHitscan(flecs::world &world, const GameContext &context, Vector3 origin, Vector3 direction);
    void FireProjectile(flecs::world &world, const GameContext &context, Vector3 origin, Vector3 direction, float charge);
    void FireMelee(flecs::world &world, const GameContext &context);

    flecs::query<> m_livingEnemies;     // Enemy, Transform, Collider, without Dead
};
