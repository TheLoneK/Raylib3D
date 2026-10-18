#include "bosses/SkullLord.h"
#include "bosses/Bosses.h"
#include "assets/AssetLibrary.h"
#include "assets/AssetSlots.h"
#include "combat/DamageZones.h"
#include "combat/Projectiles.h"
#include "core/MathUtils.h"
#include "core/Render.h"
#include "core/RenderAssets.h"
#include "player/Player.h"
#include "world/Level.h"
#include "raymath.h"

namespace
{
    // Combat stats like health, speed and bolt damage live in EnemyDefs.cpp
    constexpr float HoverHeight = 1.8f;             // Bottom of the skull above the floor
    constexpr int BoltsPerVolley = 3;
    constexpr float BoltSpreadDegrees = 7.0f;
    constexpr Color BoltColor = { 120, 255, 140, 255 };
    constexpr float ZoneInterval = 5.5f;
    constexpr int ZonesPerWave = 3;                 // One on the player, the rest around them
    constexpr float ZoneRadius = 2.3f;
    constexpr float ZoneWarmup = 1.1f;
    constexpr float ZoneActiveTime = 3.5f;
    constexpr float ZoneDamagePerSecond = 30.0f;
    constexpr float ZoneMaxDistance = 40.0f;        // Only drops zones on a player this close
    constexpr Color ZoneColor = { 170, 70, 255, 255 };
    constexpr float ChargeTime = 0.5f;              // Eyes glow / jaw opens before a volley
    constexpr float DeathFallTime = 1.0f;

    const EnemyDef &Def() { return GetEnemyDef(EnemyType::SkullLord); }

    // Imported model (asset editor): fills the skull sphere, bottom of the sphere on the model's feet
    bool DrawSkullModel(const AssetLibrary &library, Vector3 center, float radius, float yaw, Color tint, Color flashColor, float flash)
    {
        const char *slot = GetEnemySlotId(EnemyType::SkullLord);
        if (!library.HasModel(slot)) return false;

        AssetDrawParams params;
        params.placement = AssetPlacement(center - Vector3{ 0.0f, radius, 0.0f }, yaw);
        params.fitSize = GetEnemyFitSize(EnemyType::SkullLord);
        params.tint = tint;
        params.flashColor = flashColor;
        params.flash = flash;
        params.time = static_cast<float>(GetTime());
        library.DrawSlot(slot, params);
        return true;
    }

    Vector3 GetMouth(const WorldTransform &transform, const Collider &collider)
    {
        Vector3 forward = MathUtils::DirectionFromYaw(transform.yaw);
        return Bosses::GetCenter(transform, collider) + forward*(collider.radius*0.9f) - Vector3{ 0.0f, collider.radius*0.45f, 0.0f };
    }

    void FireVolley(flecs::world &world, const WorldTransform &transform, const Collider &collider, const Player &player)
    {
        const EnemyDef &def = Def();
        Vector3 mouth = GetMouth(transform, collider);
        Vector3 aim = Vector3Normalize(player.GetCenter() - mouth);

        for (int i = 0; i < BoltsPerVolley; i++)
        {
            float offset = (static_cast<float>(i) - static_cast<float>(BoltsPerVolley - 1)*0.5f)*BoltSpreadDegrees*DEG2RAD;
            Vector3 direction = MathUtils::RandomConeDirection(Vector3RotateByAxisAngle(aim, Vector3{ 0.0f, 1.0f, 0.0f }, offset), def.aimSpreadDegrees);
            Projectiles::Spawn(world, Team::Enemy, mouth, direction*def.projectileSpeed, def.projectileRadius, def.attackDamage, 4.0f, BoltColor);
        }
    }

    void SpawnZonesAroundPlayer(flecs::world &world, const Level &level, const Player &player)
    {
        Vector3 center = player.GetPosition();
        float startAngle = MathUtils::RandomFloat(0.0f, 2.0f*PI);

        for (int i = 0; i < ZonesPerWave; i++)
        {
            Vector3 position = center;
            if (i > 0)
            {
                float angle = startAngle + static_cast<float>(i)*2.0f*PI/static_cast<float>(ZonesPerWave - 1);
                position = center + MathUtils::DirectionFromYaw(angle)*MathUtils::RandomFloat(3.5f, 5.5f);
            }
            if (level.IsSolidAt(position)) continue;

            DamageZones::Spawn(world, Team::Enemy, position, ZoneRadius, ZoneDamagePerSecond, ZoneWarmup, ZoneActiveTime, ZoneColor);
        }
    }

    void DrawSkull(const RenderAssets &assets, Vector3 center, float radius, float yaw, Color bone, float charge, bool dead)
    {
        Vector3 up = { 0.0f, 1.0f, 0.0f };
        Vector3 forward = MathUtils::DirectionFromYaw(yaw);
        Vector3 right = MathUtils::RightFromYaw(yaw);
        Color dark = Render::ApplyFog(Color{ 25, 12, 18, 255 }, center);

        // Cranium + jaw (the jaw drops open while charging a volley)
        assets.DrawSphere(center + up*(radius*0.15f), radius, bone);
        float jawOpen = charge*radius*0.3f;
        assets.DrawBox(center - up*(radius*0.7f + jawOpen) + forward*(radius*0.25f), Vector3{ radius*1.1f, radius*0.35f, radius*1.0f }, yaw, Render::Shade(bone, 0.85f));
        assets.DrawBox(center - up*(radius*0.5f + jawOpen*0.5f) + forward*(radius*0.72f), Vector3{ radius*0.8f, radius*0.12f, radius*0.1f }, yaw, Render::Shade(bone, 1.1f));

        // Eye sockets with glowing pupils, and the nose
        for (float side : { -1.0f, 1.0f })
        {
            Vector3 socket = center + forward*(radius*0.72f) + right*(radius*0.38f*side) + up*(radius*0.08f);
            assets.DrawSphere(socket, radius*0.3f, dark);
            if (!dead)
            {
                Color glow = ColorLerp(Color{ 255, 40, 40, 255 }, Color{ 255, 240, 120, 255 }, charge);
                assets.DrawSphere(socket + forward*(radius*0.14f), radius*(0.11f + 0.06f*charge), glow);
            }
        }
        assets.DrawBox(center + forward*(radius*0.93f) - up*(radius*0.25f), Vector3{ radius*0.18f, radius*0.25f, radius*0.12f }, yaw, dark);
    }
}

namespace SkullLord
{
    flecs::entity Spawn(flecs::world &world, Vector3 position)
    {
        return Bosses::CreateBossEntity(world, BossType::SkullLord, EnemyType::SkullLord, position, HoverHeight)
            .set(SkullLordAttacks{ 1.0f, 2.5f });
    }

    void RegisterSystems(flecs::world &world, SystemList &drawSystems)
    {
        world.system<const WorldTransform, const Boss, SkullLordAttacks, const Collider>("SkullLordAttacks")
            .without<Dead>()
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Boss &boss, SkullLordAttacks &attacks, const Collider &collider) {
                const GameContext &context = Ecs::GetContext(it);
                const Player &player = *context.player;
                if (!Bosses::IsActive(boss, player)) return;

                flecs::world world = it.world();
                float deltaTime = it.delta_time();

                // Bolt volley every attackCooldown seconds while the player is in sight
                attacks.boltTimer -= deltaTime;
                if (attacks.boltTimer <= 0.0f)
                {
                    if (boss.canSeePlayer)
                    {
                        FireVolley(world, transform, collider, player);
                        attacks.boltTimer = Def().attackCooldown;
                    }
                    else attacks.boltTimer = 0.3f;
                }

                attacks.zoneTimer -= deltaTime;
                if (attacks.zoneTimer <= 0.0f)
                {
                    attacks.zoneTimer = ZoneInterval;
                    if (MathUtils::FlatDistance(transform.position, player.GetPosition()) <= ZoneMaxDistance) SpawnZonesAroundPlayer(world, *context.level, player);
                }
            });

        drawSystems.push_back(world.system<const WorldTransform, const Boss, const SkullLordAttacks, const Collider, const EnemyVisual>("DrawSkullLord")
            .kind(0)
            .without<Dead>()
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Boss &boss, const SkullLordAttacks &attacks, const Collider &collider, const EnemyVisual &visual) {
                const GameContext &context = Ecs::GetContext(it);
                const RenderAssets &assets = *context.assets;
                Vector3 center = Bosses::GetCenter(transform, collider);
                float charge = (boss.engaged && (attacks.boltTimer < ChargeTime) && boss.canSeePlayer) ? 1.0f - attacks.boltTimer/ChargeTime : 0.0f;

                Color bone = ColorLerp(Def().color, WHITE, visual.hitFlash);
                Color flashColor = (visual.hitFlash > 0.0f) ? WHITE : ZoneColor;
                if (!DrawSkullModel(*context.library, center, collider.radius, transform.yaw, WHITE, flashColor, std::max(visual.hitFlash, charge*0.4f)))
                {
                    DrawSkull(assets, center, collider.radius, transform.yaw, Render::ApplyFog(bone, center), charge, false);
                }

                // Shadow on the floor and a faint aura that brightens while charging
                assets.DrawCylinder(Vector3{ transform.position.x, 0.02f, transform.position.z }, collider.radius*0.9f, 0.01f, Fade(BLACK, 0.35f));
                assets.DrawSphere(center, collider.radius*1.3f, Fade(ZoneColor, 0.10f + 0.15f*charge));
            }));

        drawSystems.push_back(world.system<const WorldTransform, const Collider, const Lifetime>("DrawSkullLordCorpse")
            .kind(0)
            .with<SkullLordAttacks>()
            .with<Dead>()
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Collider &collider, const Lifetime &lifetime) {
                // Drops to the floor and goes dark
                float fall = Clamp((lifetime.total - lifetime.remaining)/DeathFallTime, 0.0f, 1.0f);
                float height = Lerp(transform.position.y + collider.height*0.5f, collider.radius*0.9f, fall*fall);
                Vector3 center = Vector3{ transform.position.x, height, transform.position.z };
                Color bone = Render::ApplyFog(Render::Shade(Def().color, 0.5f), center);
                const GameContext &context = Ecs::GetContext(it);
                if (!DrawSkullModel(*context.library, center, collider.radius, transform.yaw, Color{ 120, 120, 120, 255 }, WHITE, 0.0f))
                {
                    DrawSkull(*context.assets, center, collider.radius, transform.yaw, bone, 0.0f, true);
                }
            }));
    }
}
