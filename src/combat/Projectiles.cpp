#include "combat/Projectiles.h"
#include "combat/Effects.h"
#include "core/Render.h"
#include "core/RenderAssets.h"
#include "ecs/GameContext.h"
#include "enemies/Enemies.h"
#include "player/Player.h"
#include "world/Level.h"
#include "world/SpatialGrid.h"
#include "raymath.h"
#include <algorithm>

namespace
{
    // Shaft, head and fletching; tip is the front point, direction is the flight direction
    void DrawArrow(const RenderAssets &assets, Vector3 tip, Vector3 direction)
    {
        constexpr float Length = 0.9f;
        Vector3 tail = tip - direction*Length;
        Color shaft = Render::ApplyFog(Color{ 150, 110, 70, 255 }, tip);
        assets.DrawCylinderBetween(tail, tip - direction*0.12f, 0.025f, shaft);
        assets.DrawCylinderBetween(tip - direction*0.14f, tip, 0.05f, Render::ApplyFog(Color{ 170, 170, 180, 255 }, tip));
        assets.DrawCylinderBetween(tail, tail + direction*0.18f, 0.06f, Render::ApplyFog(Color{ 220, 60, 50, 255 }, tip));
    }

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
        return Spawn(world, position, velocity, Projectile{ team, damage, radius, color, 0.0f, ProjectileShape::Orb, 1.0f }, lifetime);
    }

    flecs::entity Spawn(flecs::world &world, Vector3 position, Vector3 velocity, const Projectile &projectile, float lifetime)
    {
        return world.entity()
            .set(WorldTransform{ position, 0.0f })
            .set(Velocity{ velocity })
            .set(projectile)
            .set(Lifetime{ lifetime, lifetime });
    }

    void RegisterSystems(flecs::world &world, SystemList &drawSystems)
    {
        world.system<WorldTransform, Velocity, const Projectile>("MoveProjectiles")
            .each([](flecs::iter &it, size_t row, WorldTransform &transform, Velocity &velocity, const Projectile &projectile) {
                flecs::world world = it.world();
                const GameContext &context = world.get<GameContext>();
                velocity.value.y -= projectile.gravity*it.delta_time();     // Arcing shots (partly drawn arrows)

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
                        hitPoint = ray.position + ray.direction*enemyDistance;
                        bool headshot = Enemies::IsHeadshot(target, hitPoint);
                        float damage = projectile.damage*(headshot ? projectile.headshotMultiplier : 1.0f);
                        Enemies::RecordHit(*context.combatStats, Enemies::ApplyDamage(target, damage), headshot);
                        consumed = true;
                        wallHit.hit = false;
                    }
                }

                if (consumed)
                {
                    // Arrows that hit a wall stay stuck in it for a while
                    if ((projectile.shape == ProjectileShape::Arrow) && wallHit.hit)
                    {
                        world.entity().set(StuckArrow{ hitPoint, ray.direction }).set(Lifetime{ 10.0f, 10.0f });
                    }
                    Effects::SpawnImpact(world, hitPoint, std::max(projectile.radius*1.5f, 0.1f), projectile.color);
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
                const RenderAssets &assets = *Ecs::GetContext(it).assets;
                if (projectile.shape == ProjectileShape::Arrow)
                {
                    DrawArrow(assets, transform.position, Vector3Normalize(velocity.value));
                    return;
                }

                // Short trail behind the core makes fast projectiles readable
                DrawLine3D(transform.position - velocity.value*0.04f, transform.position, Fade(projectile.color, 0.7f));
                assets.DrawSphere(transform.position, projectile.radius, projectile.color);
            }));

        drawSystems.push_back(world.system<const StuckArrow>("DrawStuckArrows")
            .kind(0)
            .each([](flecs::iter &it, size_t, const StuckArrow &arrow) {
                if (Render::IsFullyFogged(arrow.tip, 1.0f)) return;
                DrawArrow(*Ecs::GetContext(it).assets, arrow.tip, arrow.direction);
            }));
    }
}
