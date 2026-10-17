#include "enemies/Enemies.h"
#include "core/MathUtils.h"
#include "world/Level.h"
#include "world/SpatialGrid.h"
#include "raymath.h"

namespace Enemies
{
    flecs::entity Spawn(flecs::world &world, EnemyType type, Vector3 position, float yaw, bool huntsPlayer)
    {
        const EnemyDef &def = GetEnemyDef(type);

        EnemyBrain brain = {};
        brain.state = huntsPlayer ? EnemyState::Chase : EnemyState::Idle;
        brain.huntsPlayer = huntsPlayer;
        brain.senseTimer = MathUtils::RandomFloat(0.0f, 0.2f);     // Spread sight checks across frames
        brain.stateTimer = MathUtils::RandomFloat(0.5f, 2.0f);
        brain.strafeDirection = 1.0f;
        brain.moveTarget = position;
        brain.stuckCheckPosition = position;

        return world.entity()
            .set(WorldTransform{ position, yaw })
            .set(Collider{ def.radius, def.height })
            .set(Health{ def.maxHealth, def.maxHealth })
            .set(Enemy{ type })
            .set(brain)
            .set(EnemyVisual{ 0.0f, MathUtils::RandomFloat(0.0f, 6.0f) });
    }

    int SpawnGroup(flecs::world &world, const Level &level, Vector3 avoidPosition, float minDistance,
                   int count, float gunnerFraction, bool huntsPlayer)
    {
        int spawned = 0;
        float jitter = level.GetTileSize()*0.3f;

        for (int attempt = 0; (attempt < count*50) && (spawned < count); attempt++)
        {
            CellCoord cell = { GetRandomValue(1, level.GetWidth() - 2), GetRandomValue(1, level.GetDepth() - 2) };
            if (level.IsSolid(cell)) continue;

            Vector3 position = level.CellToWorld(cell) + Vector3{ MathUtils::RandomFloat(-jitter, jitter), 0.0f, MathUtils::RandomFloat(-jitter, jitter) };
            if (Vector3Distance(position, avoidPosition) < minDistance) continue;

            bool gunner = MathUtils::RandomFloat(0.0f, 1.0f) < gunnerFraction;
            Spawn(world, gunner ? EnemyType::Gunner : EnemyType::Brute, position, MathUtils::RandomFloat(-PI, PI), huntsPlayer);
            spawned++;
        }

        return spawned;
    }

    void Alert(EnemyBrain &brain, const EnemyDef &def, Vector3 position)
    {
        brain.loseSightTimer = def.memoryTime;
        if ((brain.state == EnemyState::Idle) || (brain.state == EnemyState::Wander))
        {
            brain.state = EnemyState::Chase;
            brain.stateTimer = 0.0f;
            brain.stuckCheckPosition = position;
        }
    }

    bool ApplyDamage(flecs::entity enemy, float amount)
    {
        if (!enemy.is_alive() || enemy.has<Dead>()) return false;

        // Health can already be 0 while the Dead tag is still queued (deferred inside a system)
        Health *health = enemy.try_get_mut<Health>();
        if ((health == nullptr) || (health->current <= 0.0f)) return false;

        health->current -= amount;
        if (EnemyVisual *visual = enemy.try_get_mut<EnemyVisual>()) visual->hitFlash = 1.0f;

        if (health->current <= 0.0f)
        {
            health->current = 0.0f;
            enemy.add<Dead>();
            enemy.set(Lifetime{ CorpseLifetime, CorpseLifetime });
            return true;
        }

        EnemyBrain *brain = enemy.try_get_mut<EnemyBrain>();
        const Enemy *info = enemy.try_get<Enemy>();
        const WorldTransform *transform = enemy.try_get<WorldTransform>();
        if ((brain != nullptr) && (info != nullptr) && (transform != nullptr)) Alert(*brain, GetEnemyDef(info->type), transform->position);
        return false;
    }

    void AlertNoise(flecs::world &world, const SpatialGrid &grid, Vector3 noisePosition, float radius)
    {
        grid.QueryRadius(noisePosition, radius, [&](const GridEntry &entry) {
            if (Vector3Distance(entry.position, noisePosition) > radius) return;

            flecs::entity enemy = world.entity(entry.id);
            EnemyBrain *brain = enemy.try_get_mut<EnemyBrain>();
            const Enemy *info = enemy.try_get<Enemy>();
            if ((brain != nullptr) && (info != nullptr) && !enemy.has<Dead>()) Alert(*brain, GetEnemyDef(info->type), entry.position);
        });
    }

    BoundingBox GetBounds(const WorldTransform &transform, const Collider &collider)
    {
        Vector3 p = transform.position;
        return BoundingBox{
            Vector3{ p.x - collider.radius, p.y, p.z - collider.radius },
            Vector3{ p.x + collider.radius, p.y + collider.height, p.z + collider.radius },
        };
    }

    Vector3 GetEyePosition(const WorldTransform &transform, const Collider &collider)
    {
        return transform.position + Vector3{ 0.0f, collider.height*0.85f, 0.0f };
    }

    Vector3 GetMuzzlePosition(const WorldTransform &transform, const EnemyDef &def)
    {
        Vector3 forward = MathUtils::DirectionFromYaw(transform.yaw);
        Vector3 gunBase = transform.position + Vector3{ 0.0f, def.height*0.55f, 0.0f } + MathUtils::RightFromYaw(transform.yaw)*(def.radius*0.9f);
        return gunBase + forward*(def.radius + 0.5f);
    }
}
