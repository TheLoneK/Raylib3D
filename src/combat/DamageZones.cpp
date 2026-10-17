#include "combat/DamageZones.h"
#include "core/MathUtils.h"
#include "core/Render.h"
#include "core/RenderAssets.h"
#include "enemies/Enemies.h"
#include "player/Player.h"
#include "world/SpatialGrid.h"
#include "raymath.h"

namespace
{
    constexpr float ColumnHeight = 9.0f;
}

namespace DamageZones
{
    flecs::entity Spawn(flecs::world &world, Team team, Vector3 position, float radius, float damagePerSecond,
                        float warmup, float activeTime, Color color)
    {
        float total = warmup + activeTime;
        return world.entity()
            .set(WorldTransform{ Vector3{ position.x, 0.0f, position.z }, 0.0f })
            .set(DamageZone{ team, radius, ColumnHeight, damagePerSecond, warmup, warmup, color })
            .set(Lifetime{ total, total });
    }

    void RegisterSystems(flecs::world &world, SystemList &drawSystems)
    {
        world.system<const WorldTransform, DamageZone>("DamageZones")
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, DamageZone &zone) {
                float deltaTime = it.delta_time();
                if (zone.warmup > 0.0f)
                {
                    zone.warmup -= deltaTime;
                    return;
                }

                const GameContext &context = Ecs::GetContext(it);
                float damage = zone.damagePerSecond*deltaTime;

                if (zone.team == Team::Enemy)
                {
                    Player &player = *context.player;
                    bool inside = MathUtils::FlatDistance(transform.position, player.GetPosition()) <= zone.radius + Player::Radius;
                    if (inside && (player.GetPosition().y < zone.height)) player.TakeDamageOverTime(damage);
                }
                else
                {
                    flecs::world world = it.world();
                    context.enemyGrid->QueryRadius(transform.position, zone.radius + GetLargestEnemyRadius(), [&](const GridEntry &entry) {
                        if (MathUtils::FlatDistance(transform.position, entry.position) > zone.radius + entry.radius) return;
                        Enemies::ApplyDamage(world.entity(entry.id), damage);
                    });
                }
            });

        drawSystems.push_back(world.system<const WorldTransform, const DamageZone, const Lifetime>("DrawDamageZones")
            .kind(0)
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const DamageZone &zone, const Lifetime &lifetime) {
                const RenderAssets &assets = *Ecs::GetContext(it).assets;
                float time = static_cast<float>(GetTime());
                Vector3 base = transform.position + Vector3{ 0.0f, 0.03f, 0.0f };

                if (zone.warmup > 0.0f)
                {
                    // Warning: pulsing ring that closes in, plus a faint disc
                    float progress = 1.0f - zone.warmup/zone.warmupTotal;
                    float pulse = 0.5f + 0.5f*sinf(time*20.0f);
                    DrawCircle3D(base, zone.radius, Vector3{ 1.0f, 0.0f, 0.0f }, 90.0f, Fade(zone.color, 0.6f + 0.4f*pulse));
                    DrawCircle3D(base, zone.radius*(1.0f - progress), Vector3{ 1.0f, 0.0f, 0.0f }, 90.0f, zone.color);
                    assets.DrawCylinder(transform.position, zone.radius, 0.04f, Fade(zone.color, 0.15f + 0.15f*progress));
                    return;
                }

                // Active: tall translucent column that fades out at the end of its life
                float fade = Clamp(lifetime.remaining/0.4f, 0.0f, 1.0f);
                float flicker = 0.85f + 0.15f*sinf(time*30.0f + transform.position.x);
                assets.DrawCylinder(transform.position, zone.radius, zone.height, Fade(zone.color, 0.22f*fade*flicker));
                assets.DrawCylinder(transform.position, zone.radius*0.35f, zone.height, Fade(WHITE, 0.25f*fade*flicker));
                DrawCircle3D(base, zone.radius, Vector3{ 1.0f, 0.0f, 0.0f }, 90.0f, Fade(zone.color, fade));
            }));
    }
}
