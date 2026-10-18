#pragma once

#include "ecs/Ecs.h"
#include "ecs/Components.h"

//----------------------------------------------------------------------------------
// Moving platforms and elevators: WorldTransform + Velocity + SolidBox + PathFollower + Platform.
//   MovePlatforms       moves them along their path, carries / pushes the player     (OnUpdate)
//   CollectSolidBodies  rebuilds GameContext::solidBodies for the next player update (PostUpdate)
//   DrawPlatforms                                                                    (draw list)
// Any WorldTransform + SolidBox entity is solid to the player; PathFollower + Velocity make it move.
//----------------------------------------------------------------------------------
namespace Platforms
{
    flecs::entity Spawn(flecs::world &world, const PlatformSpawn &spawn);

    void RegisterSystems(flecs::world &world, SystemList &drawSystems);
}
