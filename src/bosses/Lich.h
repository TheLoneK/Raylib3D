#pragma once

#include "raylib.h"
#include "ecs/Ecs.h"

//----------------------------------------------------------------------------------
// Lich: a floating robed undead caster.
//   Soul bolts a burst of 3 projectiles from the staff every 3 seconds while in sight
//   Summon     calls in a group of hunting enemies
//   Zone ring  telegraphed damage zones on and around the player
//   Barrage    a damage zone under the player every 0.5s for 5s (keep moving!)
//   Shield     at half health it becomes immune and raises 3 Soul Crystals;
//              destroying all of them drops the shield
//----------------------------------------------------------------------------------
namespace Lich
{
    flecs::entity Spawn(flecs::world &world, Vector3 position);
    void RegisterSystems(flecs::world &world, SystemList &drawSystems);
}
