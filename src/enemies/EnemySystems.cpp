#include "enemies/EnemySystems.h"
#include "enemies/Enemies.h"
#include "combat/Projectiles.h"
#include "core/MathUtils.h"
#include "core/Render.h"
#include "core/RenderAssets.h"
#include "ecs/Components.h"
#include "ecs/GameContext.h"
#include "player/Player.h"
#include "world/FlowField.h"
#include "world/Level.h"
#include "world/SpatialGrid.h"
#include "raymath.h"
#include <algorithm>

namespace
{
    constexpr float ArriveDistance = 0.6f;
    constexpr int WanderRadiusCells = 4;
    constexpr float CloseAwareness = 3.0f;      // Enemies notice the player this close even from behind
    constexpr float SenseInterval = 0.2f;       // Line of sight checks per enemy: 5 per second
    constexpr float DirectChaseDistance = 10.0f;// With line of sight, walk straight at the player inside this range
    constexpr float DeathFallTime = 0.5f;

    // Everything one enemy's update needs, bundled to keep helper signatures short
    struct EnemyAgent
    {
        flecs::entity entity;
        WorldTransform &transform;
        EnemyBrain &brain;
        EnemyVisual &visual;
        const Collider &collider;
        const EnemyDef &def;
        const GameContext &context;
        float deltaTime;
    };

    //------------------------------------------------------------------------------
    // Helpers
    //------------------------------------------------------------------------------
    void SetState(EnemyAgent &agent, EnemyState state, float timer)
    {
        agent.brain.state = state;
        agent.brain.stateTimer = timer;
        agent.brain.stuckCheckTimer = 0.0f;
        agent.brain.stuckCheckPosition = agent.transform.position;
    }

    bool CanSeePlayer(const EnemyAgent &agent)
    {
        const Player &player = *agent.context.player;
        if (!player.IsAlive()) return false;

        Vector3 eye = Enemies::GetEyePosition(agent.transform, agent.collider);
        Vector3 target = player.GetEyePosition();
        float distance = Vector3Distance(eye, target);
        if (distance > agent.def.sightRange) return false;

        bool alerted = (agent.brain.state == EnemyState::Chase) || (agent.brain.state == EnemyState::Attack);
        if (!alerted && (distance > CloseAwareness))
        {
            Vector3 facing = MathUtils::DirectionFromYaw(agent.transform.yaw);
            Vector3 toPlayer = MathUtils::FlatDirection(agent.transform.position, player.GetPosition());
            if (Vector3DotProduct(facing, toPlayer) < cosf(agent.def.sightFovDegrees*0.5f*DEG2RAD)) return false;
        }

        return agent.context.level->HasLineOfSight(eye, target);
    }

    void FaceToward(EnemyAgent &agent, Vector3 target)
    {
        Vector3 direction = MathUtils::FlatDirection(agent.transform.position, target);
        if (Vector3LengthSqr(direction) < 0.0001f) return;

        float targetYaw = MathUtils::YawFromDirection(direction);
        agent.transform.yaw = MathUtils::TurnToward(agent.transform.yaw, targetYaw, agent.def.turnSpeed*agent.deltaTime);
    }

    void MoveInDirection(EnemyAgent &agent, Vector3 direction, float speed)
    {
        agent.transform.position += direction*(speed*agent.deltaTime);
        agent.context.level->ResolveCircle(agent.transform.position, agent.collider.radius);
        agent.visual.walkCycle += speed*agent.deltaTime*2.5f;
    }

    // Returns true once within stopDistance of the target
    bool MoveToward(EnemyAgent &agent, Vector3 target, float speed, float stopDistance)
    {
        if (MathUtils::FlatDistance(agent.transform.position, target) <= stopDistance) return true;

        FaceToward(agent, target);
        MoveInDirection(agent, MathUtils::FlatDirection(agent.transform.position, target), speed);
        return false;
    }

    // Checks twice a second whether the enemy actually made progress
    bool IsStuck(EnemyAgent &agent, float expectedSpeed)
    {
        agent.brain.stuckCheckTimer += agent.deltaTime;
        if (agent.brain.stuckCheckTimer < 0.5f) return false;

        float moved = MathUtils::FlatDistance(agent.transform.position, agent.brain.stuckCheckPosition);
        agent.brain.stuckCheckTimer = 0.0f;
        agent.brain.stuckCheckPosition = agent.transform.position;
        return moved < expectedSpeed*0.5f*0.25f;
    }

    void PickWanderTarget(EnemyAgent &agent)
    {
        const Level &level = *agent.context.level;
        Vector3 eye = agent.transform.position + Vector3{ 0.0f, 1.0f, 0.0f };

        // Wandering only picks points reachable in a straight line
        for (int attempt = 0; attempt < 12; attempt++)
        {
            CellCoord cell;
            if (!level.FindOpenCellNear(level.WorldToCell(agent.transform.position), WanderRadiusCells, cell)) continue;

            Vector3 target = level.CellToWorld(cell) + Vector3{ MathUtils::RandomFloat(-1.0f, 1.0f), 0.0f, MathUtils::RandomFloat(-1.0f, 1.0f) };
            if (level.HasLineOfSight(eye, target + Vector3{ 0.0f, 1.0f, 0.0f }))
            {
                agent.brain.moveTarget = target;
                return;
            }
        }

        agent.brain.moveTarget = agent.transform.position;
    }

    // Walk toward the player using the shared flow field (or straight at them when close and visible)
    void FollowFlowField(EnemyAgent &agent, float speed, float stopDistance)
    {
        const Level &level = *agent.context.level;
        const FlowField &flowField = *agent.context.flowField;
        Vector3 playerPosition = agent.context.player->GetPosition();
        float distance = MathUtils::FlatDistance(agent.transform.position, playerPosition);

        if (distance <= stopDistance)
        {
            FaceToward(agent, playerPosition);
            return;
        }

        Vector3 target = playerPosition;
        bool sameCell = (level.WorldToCell(agent.transform.position) == flowField.GetTarget());
        bool direct = sameCell || (agent.brain.canSeePlayer && (distance < DirectChaseDistance));
        if (!direct && !flowField.GetNextWaypoint(level, agent.transform.position, target)) target = playerPosition;

        MoveToward(agent, target, speed, 0.05f);
    }

    //------------------------------------------------------------------------------
    // States
    //------------------------------------------------------------------------------
    void UpdateSenses(EnemyAgent &agent)
    {
        agent.brain.senseTimer -= agent.deltaTime;
        if (agent.brain.senseTimer <= 0.0f)
        {
            agent.brain.senseTimer += SenseInterval;
            agent.brain.canSeePlayer = CanSeePlayer(agent);
        }

        if (agent.brain.canSeePlayer) Enemies::Alert(agent.brain, agent.def, agent.transform.position);
    }

    void UpdateChase(EnemyAgent &agent)
    {
        const Player &player = *agent.context.player;
        if (!player.IsAlive())
        {
            SetState(agent, EnemyState::Idle, 2.0f);
            return;
        }

        if (!agent.brain.huntsPlayer && !agent.brain.canSeePlayer)
        {
            agent.brain.loseSightTimer -= agent.deltaTime;
            if (agent.brain.loseSightTimer <= 0.0f)
            {
                SetState(agent, EnemyState::Idle, MathUtils::RandomFloat(1.5f, 3.0f));
                return;
            }
        }

        float distance = MathUtils::FlatDistance(agent.transform.position, player.GetPosition());

        if (agent.brain.canSeePlayer && (distance <= agent.def.attackRange) && (agent.brain.cooldownTimer <= 0.0f))
        {
            SetState(agent, EnemyState::Attack, 0.0f);
            agent.brain.attackTimer = agent.def.attackWindup;
            return;
        }

        if (agent.brain.canSeePlayer && (agent.def.preferredRange > 0.0f) && (distance <= agent.def.preferredRange))
        {
            // Ranged: hold distance and strafe between shots
            agent.brain.strafeTimer -= agent.deltaTime;
            if (agent.brain.strafeTimer <= 0.0f)
            {
                agent.brain.strafeDirection = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
                agent.brain.strafeTimer = MathUtils::RandomFloat(0.8f, 1.8f);
            }

            Vector3 toPlayer = MathUtils::FlatDirection(agent.transform.position, player.GetPosition());
            Vector3 direction = Vector3{ -toPlayer.z, 0.0f, toPlayer.x }*agent.brain.strafeDirection;
            if (distance < agent.def.preferredRange*0.5f) direction = Vector3Normalize(direction - toPlayer);

            FaceToward(agent, player.GetPosition());
            MoveInDirection(agent, direction, agent.def.wanderSpeed);
            return;
        }

        // No path (behind a closed door): give up and wander until something changes
        if (!agent.brain.canSeePlayer && !agent.context.flowField->IsReachable(*agent.context.level, agent.transform.position))
        {
            SetState(agent, EnemyState::Idle, MathUtils::RandomFloat(2.0f, 4.0f));
            return;
        }

        float stopDistance = agent.collider.radius + Player::Radius + 0.2f;
        FollowFlowField(agent, agent.def.chaseSpeed, stopDistance);
    }

    void PerformAttack(EnemyAgent &agent)
    {
        Player &player = *agent.context.player;

        switch (agent.def.attackKind)
        {
            case AttackKind::Melee:
            {
                // The player can dodge out of reach during the windup
                float distance = MathUtils::FlatDistance(agent.transform.position, player.GetPosition());
                Vector3 eye = Enemies::GetEyePosition(agent.transform, agent.collider);
                if ((distance <= agent.def.attackRange + 0.4f) && agent.context.level->HasLineOfSight(eye, player.GetCenter()))
                {
                    player.TakeDamage(agent.def.attackDamage);
                }
            } break;
            case AttackKind::Projectile:
            {
                Vector3 muzzle = Enemies::GetMuzzlePosition(agent.transform, agent.def);
                Vector3 direction = MathUtils::RandomConeDirection(player.GetCenter() - muzzle, agent.def.aimSpreadDegrees);
                float lifetime = agent.def.attackRange*2.0f/agent.def.projectileSpeed;

                flecs::world world = agent.entity.world();
                Projectiles::Spawn(world, Team::Enemy, muzzle, direction*agent.def.projectileSpeed,
                                   agent.def.projectileRadius, agent.def.attackDamage, lifetime, Color{ 255, 140, 40, 255 });
            } break;
        }
    }

    void UpdateEnemy(EnemyAgent &agent)
    {
        agent.brain.cooldownTimer = std::max(0.0f, agent.brain.cooldownTimer - agent.deltaTime);
        agent.brain.stateTimer -= agent.deltaTime;

        UpdateSenses(agent);

        switch (agent.brain.state)
        {
            case EnemyState::Idle:
            {
                // Hunters only set off when a path exists (closed doors can cut them off)
                bool reachable = agent.context.flowField->IsReachable(*agent.context.level, agent.transform.position);
                if (agent.brain.huntsPlayer && agent.context.player->IsAlive() && reachable) SetState(agent, EnemyState::Chase, 0.0f);
                else if (agent.brain.stateTimer <= 0.0f)
                {
                    PickWanderTarget(agent);
                    SetState(agent, EnemyState::Wander, 6.0f);
                }
            } break;
            case EnemyState::Wander:
            {
                bool arrived = MoveToward(agent, agent.brain.moveTarget, agent.def.wanderSpeed, ArriveDistance);
                bool stuck = IsStuck(agent, agent.def.wanderSpeed);
                if (arrived || stuck || (agent.brain.stateTimer <= 0.0f)) SetState(agent, EnemyState::Idle, MathUtils::RandomFloat(1.0f, 2.5f));
            } break;
            case EnemyState::Chase:
            {
                UpdateChase(agent);
            } break;
            case EnemyState::Attack:
            {
                FaceToward(agent, agent.context.player->GetPosition());
                agent.brain.attackTimer -= agent.deltaTime;
                if (agent.brain.attackTimer <= 0.0f)
                {
                    PerformAttack(agent);
                    agent.brain.cooldownTimer = agent.def.attackCooldown;
                    SetState(agent, EnemyState::Chase, 0.0f);
                }
            } break;
        }
    }

    //------------------------------------------------------------------------------
    // Drawing
    //------------------------------------------------------------------------------
    float GetWindup(const EnemyBrain &brain, const EnemyDef &def)
    {
        if ((brain.state != EnemyState::Attack) || (def.attackWindup <= 0.0f)) return 0.0f;
        return 1.0f - brain.attackTimer/def.attackWindup;
    }

    void DrawEnemy(const RenderAssets &assets, const WorldTransform &transform, const Collider &collider,
                   const EnemyDef &def, const EnemyBrain &brain, const EnemyVisual &visual)
    {
        float windup = GetWindup(brain, def);

        // Telegraph attacks by glowing toward yellow, flash white when hit
        Color body = ColorLerp(def.color, Color{ 255, 220, 80, 255 }, windup*0.7f);
        if (visual.hitFlash > 0.0f) body = ColorLerp(body, WHITE, visual.hitFlash);
        body = Render::ApplyFog(body, transform.position);
        Color dark = Render::Shade(body, 0.6f);

        Vector3 forward = MathUtils::DirectionFromYaw(transform.yaw);
        Vector3 right = MathUtils::RightFromYaw(transform.yaw);
        float radius = collider.radius;
        float height = collider.height;
        float bob = fabsf(sinf(visual.walkCycle))*0.06f;

        Vector3 chest = transform.position + Vector3{ 0.0f, height*0.6f + bob, 0.0f };
        Vector3 head = transform.position + Vector3{ 0.0f, height*0.82f + bob, 0.0f };
        assets.DrawCylinder(transform.position, radius*0.85f, height*0.68f + bob, body);
        assets.DrawSphere(head, radius*0.55f, body);

        // Eyes show facing direction; red once alerted
        bool alerted = (brain.state == EnemyState::Chase) || (brain.state == EnemyState::Attack);
        Color eyeColor = alerted ? Color{ 255, 40, 40, 255 } : Color{ 255, 230, 120, 255 };
        Vector3 eyeCenter = head + forward*(radius*0.45f) + Vector3{ 0.0f, radius*0.1f, 0.0f };
        assets.DrawSphere(eyeCenter + right*(radius*0.2f), radius*0.1f, eyeColor);
        assets.DrawSphere(eyeCenter - right*(radius*0.2f), radius*0.1f, eyeColor);

        switch (def.attackKind)
        {
            case AttackKind::Melee:
            {
                // Arms reach forward during the windup
                float reach = Lerp(0.5f, 1.3f, windup);
                for (float side : { -1.0f, 1.0f })
                {
                    Vector3 shoulder = chest + right*(radius*0.95f*side);
                    assets.DrawBox(shoulder + forward*(reach*0.5f), Vector3{ radius*0.35f, radius*0.35f, reach }, transform.yaw, dark);
                }
            } break;
            case AttackKind::Projectile:
            {
                Vector3 muzzle = Enemies::GetMuzzlePosition(transform, def);
                assets.DrawBox(muzzle - forward*0.45f, Vector3{ 0.14f, 0.14f, 0.9f }, transform.yaw, Render::ApplyFog(DARKGRAY, transform.position));
                if (windup > 0.0f) assets.DrawSphere(muzzle, 0.08f + windup*0.15f, Color{ 255, 160, 60, 255 });
            } break;
        }
    }

    void DrawCorpse(const RenderAssets &assets, const WorldTransform &transform, const Collider &collider, const EnemyDef &def, const Lifetime &lifetime)
    {
        float fall = Clamp((lifetime.total - lifetime.remaining)/DeathFallTime, 0.0f, 1.0f);
        Vector3 right = MathUtils::RightFromYaw(transform.yaw);
        Vector3 back = MathUtils::DirectionFromYaw(transform.yaw)*-1.0f;
        float angle = 90.0f*fall;
        float length = collider.height*0.7f;

        // Tip the body over backwards around its right axis
        Vector3 base = transform.position + Vector3{ 0.0f, collider.radius*0.5f*fall, 0.0f };
        Vector3 axis = Vector3{ 0.0f, cosf(angle*DEG2RAD), 0.0f } + back*sinf(angle*DEG2RAD);
        Color color = Render::ApplyFog(Render::Shade(def.color, 0.45f), transform.position);

        assets.DrawCylinderRotated(base, collider.radius*0.85f, length, right, angle, color);
        assets.DrawSphere(base + axis*(length + collider.radius*0.4f), collider.radius*0.5f, color);
    }
}

namespace EnemySystems
{
    void Register(flecs::world &world, SystemList &drawSystems)
    {
        world.system<WorldTransform, EnemyBrain, EnemyVisual, const Collider, const Enemy>("EnemyAI")
            .without<Dead>()
            .each([](flecs::iter &it, size_t row, WorldTransform &transform, EnemyBrain &brain, EnemyVisual &visual, const Collider &collider, const Enemy &enemy) {
                const GameContext &context = Ecs::GetContext(it);
                EnemyAgent agent = { it.entity(row), transform, brain, visual, collider, GetEnemyDef(enemy.type), context, it.delta_time() };
                UpdateEnemy(agent);
            });

        // Rebuilt after movement so separation and projectile hits see current positions
        world.system<const WorldTransform, const Collider>("BuildEnemyGrid")
            .with<Enemy>()
            .without<Dead>()
            .run([](flecs::iter &it) {
                SpatialGrid &grid = *Ecs::GetContext(it).enemyGrid;
                grid.Clear();
                while (it.next())
                {
                    auto transforms = it.field<const WorldTransform>(0);
                    auto colliders = it.field<const Collider>(1);
                    for (auto row : it) grid.Insert(it.entity(row).id(), transforms[row].position, colliders[row].radius);
                }
            });

        world.system<WorldTransform, const Collider>("EnemySeparation")
            .with<Enemy>()
            .without<Dead>()
            .without<Anchored>()
            .each([](flecs::iter &it, size_t row, WorldTransform &transform, const Collider &collider) {
                const GameContext &context = Ecs::GetContext(it);
                std::uint64_t self = it.entity(row).id();
                Vector3 push = { 0.0f, 0.0f, 0.0f };

                // Each enemy moves itself half the overlap; the neighbor does the other half
                context.enemyGrid->QueryRadius(transform.position, collider.radius + GetLargestEnemyRadius(), [&](const GridEntry &other) {
                    if (other.id == self) return;
                    float minDistance = collider.radius + other.radius;
                    float distance = MathUtils::FlatDistance(transform.position, other.position);
                    if (distance >= minDistance) return;

                    Vector3 away = (distance > 0.0001f) ? MathUtils::FlatDirection(other.position, transform.position) : MathUtils::DirectionFromYaw(static_cast<float>(self%628)*0.01f);
                    push += away*((minDistance - distance)*0.5f);
                });

                transform.position += push;

                // The player is solid: applied last so crowd pressure cannot push enemies into them
                const Player &player = *context.player;
                if (player.IsAlive())
                {
                    float minDistance = collider.radius + Player::Radius;
                    float distance = MathUtils::FlatDistance(transform.position, player.GetPosition());
                    if ((distance < minDistance) && (distance > 0.0001f)) transform.position += MathUtils::FlatDirection(player.GetPosition(), transform.position)*(minDistance - distance);
                }

                // Platforms (also the ones that moved into the enemy), then walls get the final say
                SolidBodies::PushOut(*context.solidBodies, transform.position, collider.radius, collider.height);
                context.level->ResolveCircle(transform.position, collider.radius);
            });

        world.system<EnemyVisual>("EnemyVisuals")
            .each([](flecs::iter &it, size_t, EnemyVisual &visual) {
                visual.hitFlash = std::max(0.0f, visual.hitFlash - it.delta_time()*5.0f);
            });

        drawSystems.push_back(world.system<const WorldTransform, const Collider, const Enemy, const EnemyBrain, const EnemyVisual>("DrawEnemies")
            .kind(0)
            .without<Dead>()
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Collider &collider, const Enemy &enemy, const EnemyBrain &brain, const EnemyVisual &visual) {
                if (Render::IsFullyFogged(transform.position, collider.height)) return;
                DrawEnemy(*Ecs::GetContext(it).assets, transform, collider, GetEnemyDef(enemy.type), brain, visual);
            }));

        drawSystems.push_back(world.system<const WorldTransform, const Collider, const Enemy, const Lifetime>("DrawCorpses")
            .kind(0)
            .with<Dead>()
            .without<Boss>()
            .without<LichCrystal>()
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Collider &collider, const Enemy &enemy, const Lifetime &lifetime) {
                if (Render::IsFullyFogged(transform.position, collider.height)) return;
                DrawCorpse(*Ecs::GetContext(it).assets, transform, collider, GetEnemyDef(enemy.type), lifetime);
            }));
    }
}
