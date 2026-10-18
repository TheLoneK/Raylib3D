#pragma once

#include "raylib.h"
#include "ecs/Ecs.h"

//----------------------------------------------------------------------------------
// Skull Lord: a floating skull. 3-bolt volley every 2 seconds while it sees the
// player, and telegraphed damage zones dropped around the player.
//----------------------------------------------------------------------------------
namespace SkullLord
{
    flecs::entity Spawn(flecs::world &world, Vector3 position);
    void RegisterSystems(flecs::world &world, SystemList &drawSystems);
}
