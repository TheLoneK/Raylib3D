#pragma once

#include "ecs/Ecs.h"
#include "ecs/Components.h"

//----------------------------------------------------------------------------------
// Damage zones: a ring on the floor warns first, then a tall column hurts anyone
// of the other team standing inside. Entities: WorldTransform + DamageZone + Lifetime.
//   DamageZones      warmup + damage over time   (OnUpdate)
//   DrawDamageZones  (draw list, translucent - registered last)
//----------------------------------------------------------------------------------
namespace DamageZones
{
    flecs::entity Spawn(flecs::world &world, Team team, Vector3 position, float radius, float damagePerSecond,
                        float warmup, float activeTime, Color color);

    void RegisterSystems(flecs::world &world, SystemList &drawSystems);
}
