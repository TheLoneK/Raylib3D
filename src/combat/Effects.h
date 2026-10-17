#pragma once

#include "ecs/Ecs.h"
#include "ecs/Components.h"

//----------------------------------------------------------------------------------
// Short lived visual effects. Each is an entity with a Lifetime, so the generic
// Lifetime system cleans them up.
//----------------------------------------------------------------------------------
namespace Effects
{
    void SpawnTracer(flecs::world &world, Vector3 start, Vector3 end, Color color);
    void SpawnImpact(flecs::world &world, Vector3 position, float size, Color color);

    void RegisterSystems(flecs::world &world, SystemList &drawSystems);
}
