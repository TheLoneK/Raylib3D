#pragma once

#include "ecs/Ecs.h"
#include "ecs/Components.h"

//----------------------------------------------------------------------------------
// Moving platforms and elevators: WorldTransform + Velocity + SolidBox + PathFollower + Platform.
//   MovePlatforms       moves them along their path, carries / pushes the player     (OnUpdate)
//   CollectSolidBodies  rebuilds GameContext::solidBodies (player, enemies, ray queries) (OnUpdate)
//   DrawPlatforms                                                                    (draw list)
//   RunPlatformsOnTrigger  a Triggered event on its channel starts / stops it        (observer)
// Any WorldTransform + SolidBox entity is solid to the player, enemies and shots;
// PathFollower + Velocity make it move. Platforms with a channel wait for a switch.
//----------------------------------------------------------------------------------
namespace Platforms
{
    flecs::entity Spawn(flecs::world &world, const PlatformSpawn &spawn);

    void RegisterSystems(flecs::world &world, SystemList &drawSystems);
}
