#pragma once

#include "ecs/Ecs.h"
#include "ecs/Components.h"

//----------------------------------------------------------------------------------
// Pickups are entities: WorldTransform + Pickup.
//   CollectPickups  applies them when the player walks over them   (OnUpdate)
//   DrawPickups                                                    (draw list)
//----------------------------------------------------------------------------------
namespace Pickups
{
    inline constexpr int HealthAmount = 25;
    inline constexpr int DamageBoostSeconds = 15;

    flecs::entity Spawn(flecs::world &world, Vector3 position, PickupType type, WeaponType weapon = WeaponType::None, int link = -1);
    void SpawnEnemyDrop(flecs::world &world, Vector3 position, bool isBoss);    // Random chance of ammo/health

    void RegisterSystems(flecs::world &world, SystemList &drawSystems);
}
