#include "enemies/BossSystems.h"
#include "combat/DamageZones.h"
#include "combat/Projectiles.h"
#include "core/MathUtils.h"
#include "core/MessageFeed.h"
#include "core/Render.h"
#include "core/RenderAssets.h"
#include "ecs/Components.h"
#include "player/Player.h"
#include "world/FlowField.h"
#include "world/Level.h"
#include "raymath.h"
#include <string>

namespace
{
    // Boss tuning (combat stats like health, speed and bolt damage live in EnemyDefs.cpp)
    constexpr float HoverHeight = 1.8f;             // Bottom of the skull above the floor
    constexpr float SenseInterval = 0.15f;
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

    const EnemyDef &BossDef() { return GetEnemyDef(EnemyType::SkullBoss); }

    Vector3 GetSkullCenter(const WorldTransform &transform, const Collider &collider)
    {
        return transform.position + Vector3{ 0.0f, collider.height*0.5f, 0.0f };
    }

    Vector3 GetMouth(const WorldTransform &transform, const Collider &collider)
    {
        Vector3 forward = MathUtils::DirectionFromYaw(transform.yaw);
        return GetSkullCenter(transform, collider) + forward*(collider.radius*0.9f) - Vector3{ 0.0f, collider.radius*0.45f, 0.0f };
    }

    void FireVolley(flecs::world &world, const WorldTransform &transform, const Collider &collider, const Player &player)
    {
        const EnemyDef &def = BossDef();
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

    void UpdateBoss(flecs::iter &it, WorldTransform &transform, Boss &boss, const Collider &collider, const Health &health)
    {
        const GameContext &context = Ecs::GetContext(it);
        const EnemyDef &def = BossDef();
        const Level &level = *context.level;
        const Player &player = *context.player;
        float deltaTime = it.delta_time();
        Vector3 center = GetSkullCenter(transform, collider);

        boss.bobPhase += deltaTime;
        boss.senseTimer -= deltaTime;
        if (boss.senseTimer <= 0.0f)
        {
            boss.senseTimer = SenseInterval;
            Vector3 eye = player.GetEyePosition();
            boss.canSeePlayer = player.IsAlive() && (Vector3Distance(center, eye) <= def.sightRange) && level.HasLineOfSight(center, eye);
        }

        if (!boss.engaged && (boss.canSeePlayer || (health.current < health.max)))
        {
            boss.engaged = true;
            boss.boltTimer = 1.0f;
            boss.zoneTimer = 2.5f;
            context.messages->Push(std::string("The ") + def.name + " awakens!", ZoneColor, 4.0f);
        }

        transform.position.y = HoverHeight + sinf(boss.bobPhase*1.6f)*0.25f;
        if (!boss.engaged || !player.IsAlive()) return;

        // Movement: hold range and drift sideways while in sight, otherwise follow the flow field
        float distance = MathUtils::FlatDistance(transform.position, player.GetPosition());
        Vector3 move = { 0.0f, 0.0f, 0.0f };
        if (boss.canSeePlayer)
        {
            Vector3 toPlayer = MathUtils::FlatDirection(transform.position, player.GetPosition());
            if (distance > def.preferredRange + 3.0f) move = toPlayer;
            else if (distance < def.preferredRange - 5.0f) move = toPlayer*-1.0f;

            boss.strafeTimer -= deltaTime;
            if (boss.strafeTimer <= 0.0f)
            {
                boss.strafeDirection = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
                boss.strafeTimer = MathUtils::RandomFloat(1.5f, 3.0f);
            }
            move += Vector3{ -toPlayer.z, 0.0f, toPlayer.x }*(0.6f*boss.strafeDirection);
        }
        else
        {
            Vector3 waypoint;
            if (context.flowField->GetNextWaypoint(level, transform.position, waypoint)) move = MathUtils::FlatDirection(transform.position, waypoint);
        }

        if (Vector3LengthSqr(move) > 0.0001f)
        {
            transform.position += Vector3Normalize(move)*(def.chaseSpeed*deltaTime);
            level.ResolveCircle(transform.position, collider.radius);
        }

        float targetYaw = MathUtils::YawFromDirection(MathUtils::FlatDirection(transform.position, player.GetPosition()));
        transform.yaw = MathUtils::TurnToward(transform.yaw, targetYaw, def.turnSpeed*deltaTime);

        flecs::world world = it.world();

        // Bolt volley every attackCooldown seconds while the player is in sight
        boss.boltTimer -= deltaTime;
        if (boss.boltTimer <= 0.0f)
        {
            if (boss.canSeePlayer)
            {
                FireVolley(world, transform, collider, player);
                boss.boltTimer = def.attackCooldown;
            }
            else boss.boltTimer = 0.3f;
        }

        // Damage zones around the player
        boss.zoneTimer -= deltaTime;
        if (boss.zoneTimer <= 0.0f)
        {
            boss.zoneTimer = ZoneInterval;
            if (distance <= ZoneMaxDistance) SpawnZonesAroundPlayer(world, level, player);
        }
    }

    //------------------------------------------------------------------------------
    // Drawing
    //------------------------------------------------------------------------------
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

namespace BossSystems
{
    flecs::entity Spawn(flecs::world &world, Vector3 position)
    {
        const EnemyDef &def = BossDef();
        Boss boss = {};
        boss.senseTimer = 0.0f;
        boss.boltTimer = 1.0f;
        boss.zoneTimer = 2.5f;
        boss.strafeDirection = 1.0f;

        return world.entity()
            .set(WorldTransform{ Vector3{ position.x, HoverHeight, position.z }, 0.0f })
            .set(Collider{ def.radius, def.height })
            .set(Health{ def.maxHealth, def.maxHealth })
            .set(Enemy{ EnemyType::SkullBoss })
            .set(EnemyVisual{ 0.0f, 0.0f })
            .set(boss);
    }

    void Register(flecs::world &world, SystemList &drawSystems)
    {
        world.system<WorldTransform, Boss, const Collider, const Health>("BossAI")
            .without<Dead>()
            .each([](flecs::iter &it, size_t, WorldTransform &transform, Boss &boss, const Collider &collider, const Health &health) {
                UpdateBoss(it, transform, boss, collider, health);
            });

        drawSystems.push_back(world.system<const WorldTransform, const Boss, const Collider, const EnemyVisual>("DrawBoss")
            .kind(0)
            .without<Dead>()
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Boss &boss, const Collider &collider, const EnemyVisual &visual) {
                const RenderAssets &assets = *Ecs::GetContext(it).assets;
                Vector3 center = GetSkullCenter(transform, collider);
                float charge = (boss.engaged && (boss.boltTimer < ChargeTime) && boss.canSeePlayer) ? 1.0f - boss.boltTimer/ChargeTime : 0.0f;

                Color bone = ColorLerp(BossDef().color, WHITE, visual.hitFlash);
                DrawSkull(assets, center, collider.radius, transform.yaw, Render::ApplyFog(bone, center), charge, false);

                // Shadow on the floor and a faint aura that brightens while charging
                assets.DrawCylinder(Vector3{ transform.position.x, 0.02f, transform.position.z }, collider.radius*0.9f, 0.01f, Fade(BLACK, 0.35f));
                assets.DrawSphere(center, collider.radius*1.3f, Fade(ZoneColor, 0.10f + 0.15f*charge));
            }));

        drawSystems.push_back(world.system<const WorldTransform, const Collider, const Lifetime>("DrawBossCorpse")
            .kind(0)
            .with<Boss>()
            .with<Dead>()
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Collider &collider, const Lifetime &lifetime) {
                // Drops to the floor and goes dark
                float fall = Clamp((lifetime.total - lifetime.remaining)/DeathFallTime, 0.0f, 1.0f);
                float height = Lerp(transform.position.y + collider.height*0.5f, collider.radius*0.9f, fall*fall);
                Vector3 center = Vector3{ transform.position.x, height, transform.position.z };
                Color bone = Render::ApplyFog(Render::Shade(BossDef().color, 0.5f), center);
                DrawSkull(*Ecs::GetContext(it).assets, center, collider.radius, transform.yaw, bone, 0.0f, true);
            }));
    }
}
