#include "bosses/Bosses.h"
#include "bosses/Lich.h"
#include "bosses/SkullLord.h"
#include "core/MathUtils.h"
#include "core/MessageFeed.h"
#include "player/Player.h"
#include "world/FlowField.h"
#include "world/Level.h"
#include "raymath.h"
#include <string>

namespace
{
    constexpr float SenseInterval = 0.15f;
    constexpr Color AwakenColor = { 170, 70, 255, 255 };

    void UpdateMovement(flecs::iter &it, WorldTransform &transform, Boss &boss, const Collider &collider, const Health &health, const Enemy &enemy)
    {
        const GameContext &context = Ecs::GetContext(it);
        const EnemyDef &def = GetEnemyDef(enemy.type);
        const Level &level = *context.level;
        const Player &player = *context.player;
        float deltaTime = it.delta_time();
        Vector3 center = Bosses::GetCenter(transform, collider);

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
            context.messages->Push(std::string("The ") + def.name + " awakens!", AwakenColor, 4.0f);
        }

        transform.position.y = boss.hoverHeight + sinf(boss.bobPhase*1.6f)*0.25f;
        if (!Bosses::IsActive(boss, player)) return;

        // Hold range and drift sideways while in sight, otherwise follow the flow field
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
            SolidBodies::PushOut(*context.solidBodies, transform.position, collider.radius, collider.height);
            level.ResolveCircle(transform.position, collider.radius);
        }

        float targetYaw = MathUtils::YawFromDirection(MathUtils::FlatDirection(transform.position, player.GetPosition()));
        transform.yaw = MathUtils::TurnToward(transform.yaw, targetYaw, def.turnSpeed*deltaTime);
    }
}

namespace Bosses
{
    flecs::entity Spawn(flecs::world &world, BossType type, Vector3 position)
    {
        switch (type)
        {
            case BossType::SkullLord: return SkullLord::Spawn(world, position);
            case BossType::Lich: return Lich::Spawn(world, position);
            case BossType::Count: break;
        }
        return flecs::entity();
    }

    flecs::entity SpawnRandom(flecs::world &world, Vector3 position)
    {
        int type = GetRandomValue(0, static_cast<int>(BossType::Count) - 1);
        return Spawn(world, static_cast<BossType>(type), position);
    }

    flecs::entity CreateBossEntity(flecs::world &world, BossType type, EnemyType enemyType, Vector3 position, float hoverHeight)
    {
        const EnemyDef &def = GetEnemyDef(enemyType);
        Boss boss = {};
        boss.type = type;
        boss.hoverHeight = hoverHeight;
        boss.strafeDirection = 1.0f;

        return world.entity()
            .set(WorldTransform{ Vector3{ position.x, hoverHeight, position.z }, 0.0f })
            .set(Collider{ def.radius, def.height })
            .set(Health{ def.maxHealth, def.maxHealth })
            .set(Enemy{ enemyType })
            .set(EnemyVisual{ 0.0f, 0.0f })
            .set(boss);
    }

    Vector3 GetCenter(const WorldTransform &transform, const Collider &collider)
    {
        return transform.position + Vector3{ 0.0f, collider.height*0.5f, 0.0f };
    }

    bool IsActive(const Boss &boss, const Player &player)
    {
        return boss.engaged && player.IsAlive();
    }

    Vector3 FindOpenSpotNear(const Level &level, Vector3 center, float angle, float distance)
    {
        Vector3 ground = Vector3{ center.x, 0.0f, center.z };
        for (float scale : { 1.0f, 0.75f, 0.5f, 0.25f })
        {
            Vector3 spot = ground + MathUtils::DirectionFromYaw(angle)*(distance*scale);
            if (!level.IsSolidAt(spot) && level.HasLineOfSight(ground + Vector3{ 0.0f, 1.0f, 0.0f }, spot + Vector3{ 0.0f, 1.0f, 0.0f })) return spot;
        }
        return ground;
    }

    void RegisterSystems(flecs::world &world, SystemList &drawSystems)
    {
        world.system<WorldTransform, Boss, const Collider, const Health, const Enemy>("BossMovement")
            .without<Dead>()
            .each([](flecs::iter &it, size_t, WorldTransform &transform, Boss &boss, const Collider &collider, const Health &health, const Enemy &enemy) {
                UpdateMovement(it, transform, boss, collider, health, enemy);
            });

        SkullLord::RegisterSystems(world, drawSystems);
        Lich::RegisterSystems(world, drawSystems);
    }
}
