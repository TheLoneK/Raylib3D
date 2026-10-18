#pragma once

#include "flecs.h"
#include "ecs/Components.h"

class Level;
class SpatialGrid;

//----------------------------------------------------------------------------------
// Enemy helpers used outside the enemy systems (spawning, damage, alerts)
//----------------------------------------------------------------------------------
enum class DamageResult
{
    None,           // Already dead / not damageable
    Immune,         // Hit an invulnerable target
    Damaged,
    Killed
};

struct CombatStats;

namespace Enemies
{
    inline constexpr float CorpseLifetime = 12.0f;
    inline constexpr float BossCorpseLifetime = 60.0f;

    flecs::entity Spawn(flecs::world &world, EnemyType type, Vector3 position, float yaw, bool huntsPlayer);

    // Spawns count enemies on random open cells at least minDistance away from avoidPosition
    int SpawnGroup(flecs::world &world, const Level &level, Vector3 avoidPosition, float minDistance,
                   int count, float gunnerFraction, bool huntsPlayer);

    DamageResult ApplyDamage(flecs::entity enemy, float amount);
    void RecordHit(CombatStats &stats, DamageResult result, bool headshot);     // Feeds the HUD hit markers
    bool IsHeadshot(flecs::entity enemy, Vector3 hitPoint);
    void OnKilled(flecs::entity enemy);                     // Corpse timer, loot drop, boss message
    void Alert(EnemyBrain &brain, const EnemyDef &def, Vector3 position);
    void AlertNoise(flecs::world &world, const SpatialGrid &grid, Vector3 noisePosition, float radius);

    BoundingBox GetBounds(const WorldTransform &transform, const Collider &collider);
    Vector3 GetEyePosition(const WorldTransform &transform, const Collider &collider);
    Vector3 GetMuzzlePosition(const WorldTransform &transform, const EnemyDef &def);
}
