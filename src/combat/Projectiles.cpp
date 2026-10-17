#include "combat/Projectiles.h"
#include "combat/Effects.h"
#include "core/RenderAssets.h"
#include "ecs/GameContext.h"
#include "enemies/Enemies.h"
#include "player/Player.h"
#include "world/Level.h"
#include "world/SpatialGrid.h"
#include "raymath.h"

namespace
{
    BoundingBox Expand(BoundingBox box, float amount)
    {
        Vector3 grow = { amount, amount, amount };
        return BoundingBox{ box.min - grow, box.max + grow };
    }

    // Closest living enemy the segment hits within maxDistance, using the spatial grid
    flecs::entity FindEnemyHit(flecs::world &world, const SpatialGrid &grid, Ray ray, float maxDistance, float padding, float &outDistance)
    {
        flecs::entity closest;
        outDistance = maxDistance;
        Vector3 middle = ray.position + ray.direction*(maxDistance*0.5f);

        grid.QueryRadius(middle, maxDistance*0.5f + padding + GetLargestEnemyRadius(), [&](const GridEntry &entry) {
            flecs::entity enemy = world.entity(entry.id);
            const WorldTransform *transform = enemy.try_get<WorldTransform>();
            const Collider *collider = enemy.try_get<Collider>();
            if ((transform == nullptr) || (collider == nullptr) || enemy.has<Dead>()) return;

            RayCollision hit = GetRayCollisionBox(ray, Expand(Enemies::GetBounds(*transform, *collider), padding));
            if (hit.hit && (hit.distance <= outDistance))
            {
                outDistance = hit.distance;
                closest = enemy;
            }
        });

        return closest;
    }
}

namespace Projectiles
{
    flecs::entity Spawn(flecs::world &world, Team team, Vector3 position, Vector3 velocity,
                        float radius, float damage, float lifetime, Color color)
    {
        return world.entity()
            .set(WorldTransform{ position, 0.0f })
            .set(Velocity{ velocity })
            .set(Projectile{ team, damage, radius, color })
            .set(Lifetime{ lifetime, lifetime });
    }

    void RegisterSystems(flecs::world &world, SystemList &drawSystems)
    {
        world.system<WorldTransform, const Velocity, const Projectile>("MoveProjectiles")
            .each([](flecs::iter &it, size_t row, WorldTransform &transform, const Velocity &velocity, const Projectile &projectile) {
                flecs::world world = it.world();
                const GameContext &context = world.get<GameContext>();

                // Sweep along this frame's movement so fast projectiles cannot tunnel
                Vector3 step = velocity.value*it.delta_time();
                float stepLength = Vector3Length(step);
                if (stepLength < 0.00001f) return;

                Ray ray = { transform.position, step/stepLength };
                RayCollision wallHit = context.level->Raycast(ray, stepLength + projectile.radius);
                float travel = wallHit.hit ? wallHit.distance : stepLength;
                Vector3 hitPoint = ray.position + ray.direction*travel;
                bool consumed = wallHit.hit;

                if (projectile.team == Team::Enemy)
                {
                    Player &player = *context.player;
                    RayCollision playerHit = GetRayCollisionBox(ray, Expand(player.GetBounds(), projectile.radius));
                    if (player.IsAlive() && playerHit.hit && (playerHit.distance <= travel))
                    {
                        player.TakeDamage(projectile.damage);
                        hitPoint = playerHit.point;
                        consumed = true;
                    }
                }
                else
                {
                    float enemyDistance = 0.0f;
                    flecs::entity target = FindEnemyHit(world, *context.enemyGrid, ray, travel, projectile.radius, enemyDistance);
                    if (target)
                    {
                        context.combatStats->hits++;
                        if (Enemies::ApplyDamage(target, projectile.damage)) context.combatStats->kills++;
                        hitPoint = ray.position + ray.direction*enemyDistance;
                        consumed = true;
                    }
                }

                if (consumed)
                {
                    Effects::SpawnImpact(world, hitPoint, projectile.radius*1.5f, projectile.color);
                    it.entity(row).destruct();
                }
                else
                {
                    transform.position = hitPoint;
                }
            });

        drawSystems.push_back(world.system<const WorldTransform, const Velocity, const Projectile>("DrawProjectiles")
            .kind(0)
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Velocity &velocity, const Projectile &projectile) {
                // Short trail behind the core makes fast projectiles readable
                DrawLine3D(transform.position - velocity.value*0.04f, transform.position, Fade(projectile.color, 0.7f));
                Ecs::GetContext(it).assets->DrawSphere(transform.position, projectile.radius, projectile.color);
            }));
    }
}
