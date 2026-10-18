#include "combat/PlayerCombat.h"
#include "combat/Effects.h"
#include "combat/Projectiles.h"
#include "core/Config.h"
#include "core/MathUtils.h"
#include "enemies/Enemies.h"
#include "player/Player.h"
#include "world/Level.h"
#include "world/SpatialGrid.h"
#include "raymath.h"
#include <algorithm>
#include <vector>

namespace
{
    constexpr Color BloodColor = { 200, 30, 30, 255 };
    constexpr Color SparkColor = { 255, 220, 140, 255 };

    // Roughly where the on-screen barrel ends, so tracers look like they leave the gun
    Vector3 GetViewmodelMuzzle(const Player &player)
    {
        Vector3 forward = player.GetForward();
        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, Vector3{ 0.0f, 1.0f, 0.0f }));
        Vector3 up = Vector3CrossProduct(right, forward);
        return player.GetEyePosition() + forward*0.6f + right*0.12f - up*0.12f;
    }
}

void PlayerCombat::Init(flecs::world &world)
{
    m_livingEnemies = world.query_builder()
        .with<Enemy>()
        .with<WorldTransform>()
        .with<Collider>()
        .without<Dead>()
        .build();
}

float PlayerCombat::GetPlayerDamage(const WeaponDef &def, const GameContext &context)
{
    return def.damage*Config::PlayerWeaponDamageScale*context.player->GetDamageMultiplier();
}

void PlayerCombat::Update(flecs::world &world, const GameContext &context, const PlayerInput &input, float deltaTime)
{
    Player &player = *context.player;
    if (!player.IsAlive() || player.GetInventory().IsSwitching()) return;

    Weapon *weapon = player.GetInventory().GetActiveWeapon();
    if ((weapon == nullptr) || !weapon->TryFire(input.firePressed, input.fireHeld, deltaTime)) return;

    const WeaponDef &def = weapon->GetDef();
    Vector3 eye = player.GetEyePosition();
    Vector3 forward = player.GetForward();

    if (def.shotKind == ShotKind::Melee)
    {
        FireMelee(world, context);
        Enemies::AlertNoise(world, *context.enemyGrid, player.GetPosition(), def.noiseRadius);
        return;
    }

    for (int pellet = 0; pellet < def.pelletsPerShot; pellet++)
    {
        Vector3 direction = MathUtils::RandomConeDirection(forward, def.spreadDegrees*player.GetSpreadScale());
        float charge = (def.fireMode == FireMode::Charge) ? weapon->GetLastShotCharge() : 1.0f;

        switch (def.shotKind)
        {
            case ShotKind::Hitscan: FireHitscan(world, context, eye, direction); break;
            case ShotKind::Projectile: FireProjectile(world, context, eye, direction, charge); break;
            case ShotKind::Melee: break;
        }
    }

    Enemies::AlertNoise(world, *context.enemyGrid, player.GetPosition(), def.noiseRadius);
}

void PlayerCombat::FireHitscan(flecs::world &world, const GameContext &context, Vector3 origin, Vector3 direction)
{
    const WeaponDef &def = context.player->GetInventory().GetActiveWeapon()->GetDef();
    Ray ray = { origin, direction };
    RayCollision wallHit = context.level->Raycast(ray, def.range);

    // Closest living enemy in front of the wall
    flecs::entity target;
    float distance = wallHit.distance;
    m_livingEnemies.each([&](flecs::entity enemy) {
        RayCollision hit = GetRayCollisionBox(ray, Enemies::GetBounds(enemy.get<WorldTransform>(), enemy.get<Collider>()));
        if (hit.hit && (hit.distance < distance))
        {
            distance = hit.distance;
            target = enemy;
        }
    });

    Vector3 end = origin + direction*distance;

    if (target)
    {
        bool headshot = Enemies::IsHeadshot(target, end);
        float damage = GetPlayerDamage(def, context)*(headshot ? def.headshotMultiplier : 1.0f);
        DamageResult result = Enemies::ApplyDamage(target, damage);
        Enemies::RecordHit(*context.combatStats, result, headshot);
        Effects::SpawnImpact(world, end, headshot ? 0.2f : 0.12f, (result == DamageResult::Immune) ? SparkColor : BloodColor);
    }
    else if (wallHit.hit)
    {
        Effects::SpawnImpact(world, end, 0.08f, SparkColor);
    }

    Effects::SpawnTracer(world, GetViewmodelMuzzle(*context.player), end, ColorLerp(def.color, SparkColor, 0.6f));
}

void PlayerCombat::FireProjectile(flecs::world &world, const GameContext &context, Vector3 origin, Vector3 direction, float charge)
{
    const WeaponDef &def = context.player->GetInventory().GetActiveWeapon()->GetDef();
    bool charged = (def.fireMode == FireMode::Charge);

    // Charged shots scale speed and damage with the draw; only a full draw flies perfectly straight
    float power = charged ? Lerp(def.minChargeScale, 1.0f, charge) : 1.0f;
    float speed = def.projectileSpeed*power;
    float gravity = (charged && (charge < 1.0f)) ? def.arcGravity : 0.0f;
    float lifetime = def.range/std::max(def.projectileSpeed*def.minChargeScale, 0.01f);

    // Spawn from the eye so projectiles always travel toward the crosshair
    Projectile projectile = {};
    projectile.team = Team::Player;
    projectile.damage = GetPlayerDamage(def, context)*power;
    projectile.radius = def.projectileRadius;
    projectile.color = def.color;
    projectile.gravity = gravity;
    projectile.shape = charged ? ProjectileShape::Arrow : ProjectileShape::Orb;
    projectile.headshotMultiplier = def.headshotMultiplier;
    Projectiles::Spawn(world, origin + direction*0.5f, direction*speed, projectile, lifetime);
}

void PlayerCombat::FireMelee(flecs::world &world, const GameContext &context)
{
    const Player &player = *context.player;
    const WeaponDef &def = player.GetInventory().GetActiveWeapon()->GetDef();
    Vector3 position = player.GetPosition();
    Vector3 eye = player.GetEyePosition();
    Vector3 facing = MathUtils::DirectionFromYaw(player.GetYaw());
    float minDot = cosf(def.spreadDegrees*0.5f*DEG2RAD);

    // Everything in a short arc in front of the player gets hit every tick
    std::vector<flecs::entity> targets;
    context.enemyGrid->QueryRadius(position, def.range + GetLargestEnemyRadius(), [&](const GridEntry &entry) {
        flecs::entity enemy = world.entity(entry.id);
        if (enemy.has<Dead>()) return;

        const WorldTransform &transform = enemy.get<WorldTransform>();
        const Collider &collider = enemy.get<Collider>();
        float distance = MathUtils::FlatDistance(position, transform.position);
        if (distance - collider.radius > def.range) return;

        bool touching = distance < collider.radius + Player::Radius + 0.3f;
        if (!touching && (Vector3DotProduct(MathUtils::FlatDirection(position, transform.position), facing) < minDot)) return;
        if (transform.position.y > eye.y + 0.5f) return;   // Out of reach (floating)

        Vector3 center = transform.position + Vector3{ 0.0f, collider.height*0.5f, 0.0f };
        if (context.level->HasLineOfSight(eye, center)) targets.push_back(enemy);
    });

    float damage = GetPlayerDamage(def, context);
    for (flecs::entity target : targets)
    {
        const WorldTransform &transform = target.get<WorldTransform>();
        Vector3 contact = transform.position + Vector3{ 0.0f, std::min(eye.y - 0.3f, target.get<Collider>().height*0.6f), 0.0f } +
                          MathUtils::FlatDirection(transform.position, position)*target.get<Collider>().radius;

        DamageResult result = Enemies::ApplyDamage(target, damage);
        Enemies::RecordHit(*context.combatStats, result, false);
        Effects::SpawnImpact(world, contact, 0.1f, (result == DamageResult::Immune) ? SparkColor : BloodColor);
    }
}
