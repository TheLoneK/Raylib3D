#pragma once

#include "ecs/Ecs.h"
#include "ecs/Components.h"

//----------------------------------------------------------------------------------
// Projectiles are entities: WorldTransform + Velocity + Projectile + Lifetime.
//   MoveProjectiles  sweep movement, wall/actor hits   (OnUpdate)
//   DrawProjectiles                                    (draw list)
//----------------------------------------------------------------------------------
namespace Projectiles
{
    flecs::entity Spawn(flecs::world &world, Team team, Vector3 position, Vector3 velocity,
                        float radius, float damage, float lifetime, Color color);

    void RegisterSystems(flecs::world &world, SystemList &drawSystems);
}
