#pragma once

#include "raylib.h"
#include "ecs/Ecs.h"

//----------------------------------------------------------------------------------
// Skull Lord boss: a floating skull that wakes up when it sees the player (or is
// hurt), fires bolt volleys every 2 seconds and drops damage zones around the player.
// It is an enemy (Enemy, Health, Collider...) so every weapon, the spatial grid and
// the damage code treat it like any other enemy; the Boss component swaps the
// regular EnemyBrain AI for these systems.
//   BossAI                    movement, bolts, damage zones   (OnUpdate)
//   DrawBoss / DrawBossCorpse                                  (draw list)
//----------------------------------------------------------------------------------
namespace BossSystems
{
    flecs::entity Spawn(flecs::world &world, Vector3 position);
    void Register(flecs::world &world, SystemList &drawSystems);
}
