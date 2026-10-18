#include "bosses/Lich.h"
#include "bosses/Bosses.h"
#include "combat/DamageZones.h"
#include "combat/Effects.h"
#include "combat/Projectiles.h"
#include "core/MathUtils.h"
#include "core/MessageFeed.h"
#include "core/Render.h"
#include "core/RenderAssets.h"
#include "enemies/Enemies.h"
#include "player/Player.h"
#include "world/Level.h"
#include "raymath.h"
#include <algorithm>
#include <string>

namespace
{
    // Ability tuning (health/speed live in EnemyDefs.cpp)
    constexpr float HoverHeight = 1.0f;             // Robe hem above the floor

    constexpr float SummonInterval = 12.0f;
    constexpr int SummonCount = 4;
    constexpr float SummonGunnerChance = 0.3f;
    constexpr int SummonEnemyCap = 260;             // Skip summoning above this many living enemies

    constexpr float ZoneRingInterval = 7.0f;
    constexpr int ZoneRingCount = 4;                // One on the player, the rest around them
    constexpr float ZoneRingRadius = 2.4f;
    constexpr float ZoneRingWarmup = 1.2f;
    constexpr float ZoneRingActive = 3.5f;
    constexpr float ZoneRingDamage = 30.0f;

    constexpr float BarrageInterval = 14.0f;        // Time between barrages
    constexpr float BarrageDuration = 5.0f;
    constexpr float BarrageTick = 0.5f;             // One zone under the player this often
    constexpr float BarrageZoneRadius = 1.8f;
    constexpr float BarrageWarmup = 0.6f;
    constexpr float BarrageActive = 2.0f;
    constexpr float BarrageDamage = 25.0f;

    constexpr int BoltsPerBurst = 3;                // Soul bolts fired one after another from the staff
    constexpr float BoltBurstGap = 0.18f;
    constexpr float BoltChargeTime = 0.4f;          // Staff glows before a burst

    constexpr int CrystalCount = 3;
    constexpr float CrystalDistance = 8.0f;
    constexpr float CrystalHover = 0.6f;

    constexpr Color ZoneColor = { 60, 220, 200, 255 };
    constexpr Color MagicColor = { 90, 230, 255, 255 };
    constexpr Color RobeTrim = { 200, 160, 60, 255 };
    constexpr Color Bone = { 225, 215, 190, 255 };
    constexpr float DeathTime = 1.5f;

    const EnemyDef &Def() { return GetEnemyDef(EnemyType::Lich); }

    // Matches the staff drawn in DrawLichBody
    Vector3 GetStaffOrb(const WorldTransform &transform)
    {
        Vector3 up = { 0.0f, 1.0f, 0.0f };
        Vector3 forward = MathUtils::DirectionFromYaw(transform.yaw);
        Vector3 right = MathUtils::RightFromYaw(transform.yaw);
        Vector3 shoulder = transform.position + up*2.15f + right*0.62f;
        Vector3 hand = shoulder + forward*0.65f - up*0.35f + right*0.1f;
        return hand + up*1.1f;
    }

    void FireSoulBolt(flecs::world &world, const WorldTransform &transform, const Player &player)
    {
        const EnemyDef &def = Def();
        Vector3 orb = GetStaffOrb(transform);
        Vector3 direction = MathUtils::RandomConeDirection(player.GetCenter() - orb, def.aimSpreadDegrees);
        Projectiles::Spawn(world, Team::Enemy, orb, direction*def.projectileSpeed, def.projectileRadius, def.attackDamage, 4.0f, MagicColor);
    }

    void SpawnZone(flecs::world &world, const Level &level, Vector3 position, float radius, float damage, float warmup, float active)
    {
        if (!level.IsSolidAt(position)) DamageZones::Spawn(world, Team::Enemy, position, radius, damage, warmup, active, ZoneColor);
    }

    void Summon(flecs::world &world, const Level &level, const WorldTransform &transform)
    {
        int living = 0;
        world.each([&](const EnemyBrain &) { living++; });
        if (living >= SummonEnemyCap) return;

        for (int i = 0; i < SummonCount; i++)
        {
            float angle = transform.yaw + static_cast<float>(i)*2.0f*PI/static_cast<float>(SummonCount);
            Vector3 spot = Bosses::FindOpenSpotNear(level, transform.position, angle, 3.5f);
            bool gunner = MathUtils::RandomFloat(0.0f, 1.0f) < SummonGunnerChance;
            Enemies::Spawn(world, gunner ? EnemyType::Gunner : EnemyType::Brute, spot, angle, true);
            Effects::SpawnImpact(world, spot + Vector3{ 0.0f, 1.0f, 0.0f }, 0.6f, MagicColor);     // Summoning flash
        }
    }

    void RaiseShield(flecs::world &world, flecs::entity lich, const Level &level, const WorldTransform &transform, MessageFeed &messages)
    {
        lich.add<Invulnerable>();
        const EnemyDef &def = GetEnemyDef(EnemyType::LichCrystal);

        for (int i = 0; i < CrystalCount; i++)
        {
            float angle = transform.yaw + static_cast<float>(i)*2.0f*PI/static_cast<float>(CrystalCount);
            Vector3 spot = Bosses::FindOpenSpotNear(level, transform.position, angle, CrystalDistance);
            world.entity()
                .set(WorldTransform{ Vector3{ spot.x, CrystalHover, spot.z }, angle })
                .set(Collider{ def.radius, def.height })
                .set(Health{ def.maxHealth, def.maxHealth })
                .set(Enemy{ EnemyType::LichCrystal })
                .set(EnemyVisual{ 0.0f, 0.0f })
                .set(LichCrystal{ lich.id(), MathUtils::RandomFloat(0.0f, 6.0f) })
                .add<Anchored>();
        }
        messages.Push("The Lich is shielded! Destroy the 3 Soul Crystals", MagicColor, 5.0f);
    }

    int CountCrystals(flecs::world &world, flecs::entity_t owner)
    {
        int count = 0;
        world.each([&](flecs::entity crystal, const LichCrystal &link) {
            if ((link.owner == owner) && !crystal.has<Dead>()) count++;
        });
        return count;
    }

    void UpdateLich(flecs::iter &it, size_t row, const WorldTransform &transform, const Boss &boss, LichAttacks &attacks, const Health &health)
    {
        const GameContext &context = Ecs::GetContext(it);
        const Player &player = *context.player;
        float deltaTime = it.delta_time();
        attacks.castFlash = std::max(0.0f, attacks.castFlash - deltaTime*2.0f);
        if (!Bosses::IsActive(boss, player)) return;

        flecs::world world = it.world();
        flecs::entity lich = it.entity(row);
        const Level &level = *context.level;

        // Shield phase at half health; it drops once every crystal is gone
        if (!attacks.crystalsSpawned && (health.current <= health.max*0.5f))
        {
            attacks.crystalsSpawned = true;
            RaiseShield(world, lich, level, transform, *context.messages);
        }
        else if (lich.has<Invulnerable>() && (CountCrystals(world, lich.id()) == 0))
        {
            lich.remove<Invulnerable>();
            context.messages->Push("The Lich's shield shatters!", MagicColor, 4.0f);
        }

        // Soul bolts: a quick burst from the staff every attackCooldown seconds while the player is in sight
        attacks.boltTimer -= deltaTime;
        if (attacks.burstRemaining > 0)
        {
            attacks.burstTick -= deltaTime;
            if (attacks.burstTick <= 0.0f)
            {
                if (boss.canSeePlayer) FireSoulBolt(world, transform, player);
                attacks.burstRemaining--;
                attacks.burstTick = BoltBurstGap;
                attacks.castFlash = std::max(attacks.castFlash, 0.7f);
            }
        }
        else
        {
            if (boss.canSeePlayer && (attacks.boltTimer < BoltChargeTime))
            {
                attacks.castFlash = std::max(attacks.castFlash, 1.0f - std::max(attacks.boltTimer, 0.0f)/BoltChargeTime);
            }
            if (attacks.boltTimer <= 0.0f)
            {
                if (boss.canSeePlayer)
                {
                    attacks.burstRemaining = BoltsPerBurst;
                    attacks.burstTick = 0.0f;
                    attacks.boltTimer = Def().attackCooldown;
                }
                else attacks.boltTimer = 0.3f;
            }
        }

        attacks.summonTimer -= deltaTime;
        if (attacks.summonTimer <= 0.0f)
        {
            attacks.summonTimer = SummonInterval;
            attacks.castFlash = 1.0f;
            Summon(world, level, transform);
        }

        attacks.zoneTimer -= deltaTime;
        if (attacks.zoneTimer <= 0.0f)
        {
            attacks.zoneTimer = ZoneRingInterval;
            attacks.castFlash = 1.0f;
            float startAngle = MathUtils::RandomFloat(0.0f, 2.0f*PI);
            for (int i = 0; i < ZoneRingCount; i++)
            {
                Vector3 position = player.GetPosition();
                if (i > 0) position += MathUtils::DirectionFromYaw(startAngle + static_cast<float>(i)*2.0f*PI/static_cast<float>(ZoneRingCount - 1))*MathUtils::RandomFloat(3.5f, 6.0f);
                SpawnZone(world, level, position, ZoneRingRadius, ZoneRingDamage, ZoneRingWarmup, ZoneRingActive);
            }
        }

        // Barrage: a zone right under the player every BarrageTick for BarrageDuration
        attacks.trailTimer -= deltaTime;
        if ((attacks.trailTimer <= 0.0f) && (attacks.trailRemaining <= 0.0f))
        {
            attacks.trailTimer = BarrageInterval;
            attacks.trailRemaining = BarrageDuration;
            attacks.trailTick = 0.0f;
            context.messages->Push("The ground beneath you burns - keep moving!", ZoneColor, 2.5f);
        }
        if (attacks.trailRemaining > 0.0f)
        {
            attacks.trailRemaining -= deltaTime;
            attacks.trailTick -= deltaTime;
            if (attacks.trailTick <= 0.0f)
            {
                attacks.trailTick += BarrageTick;
                attacks.castFlash = 0.6f;
                SpawnZone(world, level, player.GetPosition(), BarrageZoneRadius, BarrageDamage, BarrageWarmup, BarrageActive);
            }
        }
    }

    //------------------------------------------------------------------------------
    // Drawing
    //------------------------------------------------------------------------------
    void DrawLichBody(const RenderAssets &assets, const WorldTransform &transform, float bob, Color robe, float glow, float sink)
    {
        Vector3 up = { 0.0f, 1.0f, 0.0f };
        Vector3 forward = MathUtils::DirectionFromYaw(transform.yaw);
        Vector3 right = MathUtils::RightFromYaw(transform.yaw);
        Vector3 hem = transform.position + up*(bob - sink);
        auto fog = [&](Color color) { return Render::ApplyFog(color, hem); };
        Color dark = Render::Shade(robe, 0.55f);

        // Robe: a flared cone with a gold hem, narrowing to the shoulders
        assets.DrawCone(hem, 1.0f, 2.6f, robe);
        assets.DrawCylinder(hem, 1.02f, 0.1f, fog(RobeTrim));
        Vector3 shoulders = hem + up*2.15f;
        assets.DrawBox(shoulders, Vector3{ 1.25f, 0.45f, 0.6f }, transform.yaw, dark);

        // Hood, skull face and glowing eyes
        Vector3 head = shoulders + up*0.55f;
        assets.DrawSphere(head - forward*0.05f, 0.46f, dark);
        assets.DrawSphere(head + forward*0.14f, 0.32f, fog(Bone));
        Color eyes = ColorLerp(MagicColor, WHITE, glow*0.5f);
        assets.DrawSphere(head + forward*0.42f + right*0.11f + up*0.04f, 0.06f + glow*0.03f, eyes);
        assets.DrawSphere(head + forward*0.42f - right*0.11f + up*0.04f, 0.06f + glow*0.03f, eyes);

        // Sleeves reaching forward, bony hands, and a staff with a glowing orb
        for (float side : { -1.0f, 1.0f })
        {
            Vector3 shoulder = shoulders + right*(0.62f*side);
            Vector3 hand = shoulder + forward*0.65f - up*0.35f + right*(0.1f*side);
            assets.DrawCylinderBetween(shoulder, hand, 0.17f, dark);
            assets.DrawSphere(hand, 0.12f, fog(Bone));
            if (side > 0.0f)
            {
                assets.DrawCylinderBetween(hand - up*1.3f, hand + up*1.0f, 0.05f, fog(Color{ 90, 60, 35, 255 }));
                assets.DrawSphere(hand + up*1.1f, 0.16f + glow*0.08f, ColorLerp(MagicColor, WHITE, glow));
                if (glow > 0.0f) assets.DrawSphere(hand + up*1.1f, 0.4f + glow*0.3f, Fade(MagicColor, 0.25f*glow));
            }
        }
    }
}

namespace Lich
{
    flecs::entity Spawn(flecs::world &world, Vector3 position)
    {
        LichAttacks attacks = {};
        attacks.summonTimer = 4.0f;
        attacks.boltTimer = 2.0f;
        attacks.zoneTimer = 2.5f;
        attacks.trailTimer = 8.0f;
        return Bosses::CreateBossEntity(world, BossType::Lich, EnemyType::Lich, position, HoverHeight).set(attacks);
    }

    void RegisterSystems(flecs::world &world, SystemList &drawSystems)
    {
        world.system<const WorldTransform, const Boss, LichAttacks, const Health>("LichAbilities")
            .without<Dead>()
            .each([](flecs::iter &it, size_t row, const WorldTransform &transform, const Boss &boss, LichAttacks &attacks, const Health &health) {
                UpdateLich(it, row, transform, boss, attacks, health);
            });

        // Crystals spin in place; if their Lich dies they shatter too
        world.system<WorldTransform, LichCrystal>("LichCrystals")
            .without<Dead>()
            .each([](flecs::iter &it, size_t row, WorldTransform &transform, LichCrystal &crystal) {
                crystal.spin += it.delta_time();
                transform.yaw = crystal.spin;
                flecs::entity owner = it.world().entity(crystal.owner);
                if (!owner.is_alive() || owner.has<Dead>()) it.entity(row).destruct();
            });

        drawSystems.push_back(world.system<const WorldTransform, const Boss, const LichAttacks, const EnemyVisual>("DrawLich")
            .kind(0)
            .without<Dead>()
            .each([](flecs::iter &it, size_t row, const WorldTransform &transform, const Boss &boss, const LichAttacks &attacks, const EnemyVisual &visual) {
                const RenderAssets &assets = *Ecs::GetContext(it).assets;
                Color robe = Render::ApplyFog(ColorLerp(Def().color, WHITE, visual.hitFlash), transform.position);
                DrawLichBody(assets, transform, 0.0f, robe, attacks.castFlash, 0.0f);

                Vector3 center = transform.position + Vector3{ 0.0f, 1.6f, 0.0f };
                assets.DrawCylinder(Vector3{ transform.position.x, 0.02f, transform.position.z }, 1.0f, 0.01f, Fade(BLACK, 0.35f));
                if (it.entity(row).has<Invulnerable>())
                {
                    // Shield bubble
                    float pulse = 0.5f + 0.5f*sinf(boss.bobPhase*5.0f);
                    assets.DrawSphere(center, 2.0f + pulse*0.1f, Fade(MagicColor, 0.15f + 0.1f*pulse));
                }
            }));

        drawSystems.push_back(world.system<const WorldTransform, const Lifetime>("DrawLichCorpse")
            .kind(0)
            .with<LichAttacks>()
            .with<Dead>()
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Lifetime &lifetime) {
                // Sinks into the floor
                float t = Clamp((lifetime.total - lifetime.remaining)/DeathTime, 0.0f, 1.0f);
                Color robe = Render::ApplyFog(Render::Shade(Def().color, 0.5f), transform.position);
                DrawLichBody(*Ecs::GetContext(it).assets, transform, 0.0f, robe, 0.0f, t*(transform.position.y + 2.6f));
            }));

        drawSystems.push_back(world.system<const WorldTransform, const LichCrystal, const EnemyVisual, const Health>("DrawLichCrystals")
            .kind(0)
            .without<Dead>()
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const LichCrystal &crystal, const EnemyVisual &visual, const Health &health) {
                const RenderAssets &assets = *Ecs::GetContext(it).assets;
                float bob = sinf(crystal.spin*2.0f)*0.15f;
                Vector3 middle = transform.position + Vector3{ 0.0f, 1.0f + bob, 0.0f };
                float damaged = 1.0f - health.current/health.max;
                Color color = ColorLerp(ColorLerp(MagicColor, Color{ 255, 120, 120, 255 }, damaged*0.6f), WHITE, visual.hitFlash);

                // Diamond (two cones) plus a tether to the Lich it protects
                assets.DrawCone(middle, 0.55f, 0.9f, color);
                assets.DrawCone(middle, 0.55f, -0.9f, Render::Shade(color, 0.8f));
                assets.DrawSphere(middle, 0.9f, Fade(MagicColor, 0.12f));

                flecs::entity owner = it.world().entity(crystal.owner);
                if (owner.is_alive() && owner.has<WorldTransform>())
                {
                    Vector3 target = owner.get<WorldTransform>().position + Vector3{ 0.0f, 2.0f, 0.0f };
                    assets.DrawCylinderBetween(middle, target, 0.04f, Fade(MagicColor, 0.5f));
                }
            }));
    }
}
