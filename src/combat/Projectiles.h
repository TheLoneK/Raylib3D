#pragma once

#include "ecs/Ecs.h"
#include "ecs/Components.h"

//----------------------------------------------------------------------------------
// Projectiles are entities: WorldTransform + Velocity + Projectile + Lifetime.
//   MoveProjectiles  gravity, sweep movement, wall/actor hits, headshots   (OnUpdate)
//   DrawProjectiles / DrawStuckArrows                                      (draw list)
//----------------------------------------------------------------------------------
namespace Projectiles
{
    // Simple straight-flying orb (enemy shots, plasma)
    flecs::entity Spawn(flecs::world &world, Team team, Vector3 position, Vector3 velocity,
                        float radius, float damage, float lifetime, Color color);
    // Full control (arrows, gravity, headshot multiplier)
    flecs::entity Spawn(flecs::world &world, Vector3 position, Vector3 velocity, const Projectile &projectile, float lifetime);

    void RegisterSystems(flecs::world &world, SystemList &drawSystems);
}
