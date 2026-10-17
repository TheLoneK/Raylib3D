#include "enemy.h"
#include "render.h"
#include "utils.h"
#include "raymath.h"
#include <stddef.h>

#define CORPSE_LIFETIME         12.0f
#define DEATH_FALL_TIME          0.5f
#define ARRIVE_DISTANCE          0.6f
#define WANDER_RADIUS_CELLS      4
#define CLOSE_AWARENESS          3.0f   // Enemies notice the player this close even from behind

//----------------------------------------------------------------------------------
// Enemy definitions
//----------------------------------------------------------------------------------
static const EnemyDef enemyDefs[ENEMY_TYPE_COUNT] = {
    [ENEMY_TYPE_BRUTE] = {
        .name = "Brute", .attackKind = ATTACK_MELEE,
        .maxHealth = 120.0f, .radius = 0.55f, .height = 2.0f,
        .wanderSpeed = 2.5f, .chaseSpeed = 8.0f, .turnSpeed = 8.0f,
        .sightRange = 45.0f, .sightFovDegrees = 140.0f, .memoryTime = 5.0f,
        .attackRange = 2.0f, .preferredRange = 0.0f,
        .attackDamage = 20.0f, .attackWindup = 0.35f, .attackCooldown = 0.9f,
        .color = (Color){ 185, 65, 55, 255 },
    },
    [ENEMY_TYPE_GUNNER] = {
        .name = "Gunner", .attackKind = ATTACK_PROJECTILE,
        .maxHealth = 80.0f, .radius = 0.45f, .height = 1.8f,
        .wanderSpeed = 2.0f, .chaseSpeed = 4.5f, .turnSpeed = 6.0f,
        .sightRange = 60.0f, .sightFovDegrees = 120.0f, .memoryTime = 6.0f,
        .attackRange = 35.0f, .preferredRange = 14.0f,
        .attackDamage = 10.0f, .attackWindup = 0.5f, .attackCooldown = 1.3f,
        .projectileSpeed = 28.0f, .projectileRadius = 0.2f, .aimSpreadDegrees = 2.5f,
        .color = (Color){ 60, 120, 200, 255 },
    },
};

const EnemyDef *Enemy_GetDef(EnemyType type)
{
    if ((type < 0) || (type >= ENEMY_TYPE_COUNT)) type = ENEMY_TYPE_BRUTE;
    return &enemyDefs[type];
}

//----------------------------------------------------------------------------------
// Spawning / queries
//----------------------------------------------------------------------------------
void Enemies_Clear(EnemyList *list)
{
    *list = (EnemyList){ 0 };
}

Enemy *Enemies_Spawn(EnemyList *list, EnemyType type, Vector3 position, float yaw)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *enemy = &list->items[i];
        if (enemy->active) continue;

        *enemy = (Enemy){
            .active = true,
            .type = type,
            .state = ENEMY_STATE_IDLE,
            .position = position,
            .yaw = yaw,
            .health = Enemy_GetDef(type)->maxHealth,
            .moveTarget = position,
            .stateTimer = Util_RandomFloat(0.5f, 2.0f),
            .strafeDirection = 1.0f,
            .stuckCheckPosition = position,
        };
        return enemy;
    }

    return NULL;
}

bool Enemy_IsAlive(const Enemy *enemy)
{
    return enemy->active && (enemy->state != ENEMY_STATE_DEAD);
}

int Enemies_CountAlive(const EnemyList *list)
{
    int count = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (Enemy_IsAlive(&list->items[i])) count++;
    }
    return count;
}

BoundingBox Enemy_GetBounds(const Enemy *enemy)
{
    const EnemyDef *def = Enemy_GetDef(enemy->type);
    return (BoundingBox){
        .min = (Vector3){ enemy->position.x - def->radius, enemy->position.y, enemy->position.z - def->radius },
        .max = (Vector3){ enemy->position.x + def->radius, enemy->position.y + def->height, enemy->position.z + def->radius },
    };
}

Vector3 Enemy_GetEyePosition(const Enemy *enemy)
{
    return Vector3Add(enemy->position, (Vector3){ 0.0f, Enemy_GetDef(enemy->type)->height*0.85f, 0.0f });
}

Vector3 Enemy_GetHeadTop(const Enemy *enemy)
{
    return Vector3Add(enemy->position, (Vector3){ 0.0f, Enemy_GetDef(enemy->type)->height, 0.0f });
}

static Vector3 GetRightVector(float yaw)
{
    Vector3 forward = Util_DirectionFromYaw(yaw);
    return (Vector3){ -forward.z, 0.0f, forward.x };
}

static Vector3 GetMuzzlePosition(const Enemy *enemy)
{
    const EnemyDef *def = Enemy_GetDef(enemy->type);
    Vector3 forward = Util_DirectionFromYaw(enemy->yaw);
    Vector3 gunBase = Vector3Add(enemy->position, (Vector3){ 0.0f, def->height*0.55f, 0.0f });
    gunBase = Vector3Add(gunBase, Vector3Scale(GetRightVector(enemy->yaw), def->radius*0.9f));
    return Vector3Add(gunBase, Vector3Scale(forward, def->radius + 0.5f));
}

//----------------------------------------------------------------------------------
// Damage / alerts
//----------------------------------------------------------------------------------
static void SetState(Enemy *enemy, EnemyState state, float timer)
{
    enemy->state = state;
    enemy->stateTimer = timer;
    enemy->stuckCheckTimer = 0.0f;
    enemy->stuckCheckPosition = enemy->position;
}

static void Alert(Enemy *enemy, Vector3 targetPosition)
{
    enemy->lastKnownPlayerPosition = targetPosition;
    enemy->loseSightTimer = Enemy_GetDef(enemy->type)->memoryTime;
    if ((enemy->state == ENEMY_STATE_IDLE) || (enemy->state == ENEMY_STATE_WANDER)) SetState(enemy, ENEMY_STATE_CHASE, 0.0f);
}

bool Enemy_TakeDamage(Enemy *enemy, float amount, Vector3 sourcePosition)
{
    if (!Enemy_IsAlive(enemy)) return false;

    enemy->health -= amount;
    enemy->hitFlash = 1.0f;

    if (enemy->health <= 0.0f)
    {
        enemy->health = 0.0f;
        enemy->deathTimer = 0.0f;
        enemy->canSeePlayer = false;
        SetState(enemy, ENEMY_STATE_DEAD, 0.0f);
        return true;
    }

    Alert(enemy, sourcePosition);
    return false;
}

void Enemies_AlertNoise(EnemyList *list, Vector3 noisePosition, float radius)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *enemy = &list->items[i];
        if (!Enemy_IsAlive(enemy)) continue;
        if (Vector3Distance(enemy->position, noisePosition) <= radius) Alert(enemy, noisePosition);
    }
}

//----------------------------------------------------------------------------------
// Perception / movement helpers
//----------------------------------------------------------------------------------
static bool CanSeePlayer(const Enemy *enemy, const EnemyDef *def, const Level *level, const Player *player)
{
    if (!player->isAlive) return false;

    Vector3 eye = Enemy_GetEyePosition(enemy);
    Vector3 target = Player_GetEyePosition(player);
    float distance = Vector3Distance(eye, target);
    if (distance > def->sightRange) return false;

    bool alerted = (enemy->state == ENEMY_STATE_CHASE) || (enemy->state == ENEMY_STATE_ATTACK);
    if (!alerted && (distance > CLOSE_AWARENESS))
    {
        Vector3 facing = Util_DirectionFromYaw(enemy->yaw);
        Vector3 toPlayer = Util_FlatDirection(enemy->position, player->position);
        if (Vector3DotProduct(facing, toPlayer) < cosf(def->sightFovDegrees*0.5f*DEG2RAD)) return false;
    }

    return Level_HasLineOfSight(level, eye, target);
}

static void FaceToward(Enemy *enemy, const EnemyDef *def, Vector3 target, float deltaTime)
{
    Vector3 direction = Util_FlatDirection(enemy->position, target);
    if (Vector3LengthSqr(direction) < 0.0001f) return;
    enemy->yaw = Util_TurnToward(enemy->yaw, Util_YawFromDirection(direction), def->turnSpeed*deltaTime);
}

static void MoveInDirection(Enemy *enemy, const EnemyDef *def, const Level *level, Vector3 direction, float speed, float deltaTime)
{
    enemy->position = Vector3Add(enemy->position, Vector3Scale(direction, speed*deltaTime));
    Level_ResolveCircle(level, &enemy->position, def->radius, NULL);
    enemy->walkCycle += speed*deltaTime*2.5f;
}

// Returns true once within stopDistance of the target
static bool MoveToward(Enemy *enemy, const EnemyDef *def, const Level *level, Vector3 target, float speed, float stopDistance, float deltaTime)
{
    if (Util_FlatDistance(enemy->position, target) <= stopDistance) return true;

    Vector3 direction = Util_FlatDirection(enemy->position, target);
    FaceToward(enemy, def, target, deltaTime);
    MoveInDirection(enemy, def, level, direction, speed, deltaTime);
    return false;
}

// Checks twice a second whether the enemy actually made progress
static bool IsStuck(Enemy *enemy, float expectedSpeed, float deltaTime)
{
    enemy->stuckCheckTimer += deltaTime;
    if (enemy->stuckCheckTimer < 0.5f) return false;

    float moved = Util_FlatDistance(enemy->position, enemy->stuckCheckPosition);
    enemy->stuckCheckTimer = 0.0f;
    enemy->stuckCheckPosition = enemy->position;
    return (moved < expectedSpeed*0.5f*0.25f);
}

static void PickWanderTarget(Enemy *enemy, const Level *level)
{
    int cellX, cellZ;
    Level_WorldToCell(level, enemy->position, &cellX, &cellZ);
    Vector3 eye = Vector3Add(enemy->position, (Vector3){ 0.0f, 1.0f, 0.0f });

    // Only pick points we can walk to in a straight line (no pathfinding yet)
    for (int attempt = 0; attempt < 12; attempt++)
    {
        int x, z;
        if (!Level_FindOpenCellNear(level, cellX, cellZ, WANDER_RADIUS_CELLS, &x, &z)) continue;

        Vector3 target = Level_CellToWorld(level, x, z);
        target.x += Util_RandomFloat(-1.0f, 1.0f);
        target.z += Util_RandomFloat(-1.0f, 1.0f);

        if (Level_HasLineOfSight(level, eye, Vector3Add(target, (Vector3){ 0.0f, 1.0f, 0.0f })))
        {
            enemy->moveTarget = target;
            return;
        }
    }

    enemy->moveTarget = enemy->position;
}

//----------------------------------------------------------------------------------
// State machine
//----------------------------------------------------------------------------------
static void UpdateChase(Enemy *enemy, const EnemyDef *def, const Level *level, const Player *player, float deltaTime)
{
    if (!player->isAlive)
    {
        SetState(enemy, ENEMY_STATE_IDLE, 2.0f);
        return;
    }

    float distance = Util_FlatDistance(enemy->position, player->position);

    if (enemy->canSeePlayer)
    {
        if ((distance <= def->attackRange) && (enemy->cooldownTimer <= 0.0f))
        {
            SetState(enemy, ENEMY_STATE_ATTACK, 0.0f);
            enemy->attackTimer = def->attackWindup;
            return;
        }

        if ((def->preferredRange > 0.0f) && (distance <= def->preferredRange))
        {
            // Ranged: hold distance and strafe between shots
            enemy->strafeTimer -= deltaTime;
            if (enemy->strafeTimer <= 0.0f)
            {
                enemy->strafeDirection = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
                enemy->strafeTimer = Util_RandomFloat(0.8f, 1.8f);
            }

            Vector3 toPlayer = Util_FlatDirection(enemy->position, player->position);
            Vector3 side = Vector3Scale((Vector3){ -toPlayer.z, 0.0f, toPlayer.x }, enemy->strafeDirection);
            Vector3 direction = side;
            if (distance < def->preferredRange*0.5f) direction = Vector3Normalize(Vector3Subtract(side, toPlayer));

            FaceToward(enemy, def, player->position, deltaTime);
            MoveInDirection(enemy, def, level, direction, def->wanderSpeed, deltaTime);
        }
        else
        {
            float stopDistance = def->radius + PLAYER_RADIUS + 0.2f;
            MoveToward(enemy, def, level, player->position, def->chaseSpeed, stopDistance, deltaTime);
            FaceToward(enemy, def, player->position, deltaTime);
        }
        return;
    }

    // Lost sight: head to the last known position until memory runs out
    enemy->loseSightTimer -= deltaTime;
    bool arrived = MoveToward(enemy, def, level, enemy->lastKnownPlayerPosition, def->chaseSpeed, ARRIVE_DISTANCE, deltaTime);
    bool stuck = IsStuck(enemy, def->chaseSpeed, deltaTime);

    if (arrived || stuck || (enemy->loseSightTimer <= 0.0f)) SetState(enemy, ENEMY_STATE_IDLE, Util_RandomFloat(1.5f, 3.0f));
}

static void PerformAttack(Enemy *enemy, const EnemyDef *def, const Level *level, Player *player, ProjectilePool *projectiles)
{
    switch (def->attackKind)
    {
        case ATTACK_MELEE:
        {
            // The player can dodge out of reach during the windup
            float distance = Util_FlatDistance(enemy->position, player->position);
            if ((distance <= def->attackRange + 0.4f) && Level_HasLineOfSight(level, Enemy_GetEyePosition(enemy), Player_GetCenter(player)))
            {
                Player_TakeDamage(player, def->attackDamage);
            }
        } break;
        case ATTACK_PROJECTILE:
        {
            Vector3 muzzle = GetMuzzlePosition(enemy);
            Vector3 aim = Vector3Subtract(Player_GetCenter(player), muzzle);
            Vector3 direction = Util_RandomConeDirection(aim, def->aimSpreadDegrees);
            float lifetime = def->attackRange*2.0f/def->projectileSpeed;

            Projectiles_Spawn(projectiles, TEAM_ENEMY, muzzle, Vector3Scale(direction, def->projectileSpeed),
                              def->projectileRadius, def->attackDamage, lifetime, (Color){ 255, 140, 40, 255 });
        } break;
        default: break;
    }
}

static void UpdateEnemy(Enemy *enemy, const Level *level, Player *player, ProjectilePool *projectiles, float deltaTime)
{
    const EnemyDef *def = Enemy_GetDef(enemy->type);

    enemy->hitFlash = fmaxf(0.0f, enemy->hitFlash - deltaTime*5.0f);
    enemy->cooldownTimer = fmaxf(0.0f, enemy->cooldownTimer - deltaTime);
    enemy->stateTimer -= deltaTime;

    enemy->canSeePlayer = CanSeePlayer(enemy, def, level, player);
    if (enemy->canSeePlayer) Alert(enemy, player->position);

    switch (enemy->state)
    {
        case ENEMY_STATE_IDLE:
        {
            if (enemy->stateTimer <= 0.0f)
            {
                PickWanderTarget(enemy, level);
                SetState(enemy, ENEMY_STATE_WANDER, 6.0f);
            }
        } break;
        case ENEMY_STATE_WANDER:
        {
            bool arrived = MoveToward(enemy, def, level, enemy->moveTarget, def->wanderSpeed, ARRIVE_DISTANCE, deltaTime);
            bool stuck = IsStuck(enemy, def->wanderSpeed, deltaTime);
            if (arrived || stuck || (enemy->stateTimer <= 0.0f)) SetState(enemy, ENEMY_STATE_IDLE, Util_RandomFloat(1.0f, 2.5f));
        } break;
        case ENEMY_STATE_CHASE:
        {
            UpdateChase(enemy, def, level, player, deltaTime);
        } break;
        case ENEMY_STATE_ATTACK:
        {
            FaceToward(enemy, def, player->position, deltaTime);
            enemy->attackTimer -= deltaTime;
            if (enemy->attackTimer <= 0.0f)
            {
                PerformAttack(enemy, def, level, player, projectiles);
                enemy->cooldownTimer = def->attackCooldown;
                SetState(enemy, ENEMY_STATE_CHASE, 0.0f);
            }
        } break;
        default: break;
    }
}

// Keeps enemies from overlapping each other and the player
static void SeparateBodies(EnemyList *list, const Level *level, const Player *player)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *a = &list->items[i];
        if (!Enemy_IsAlive(a)) continue;
        float radiusA = Enemy_GetDef(a->type)->radius;

        for (int j = i + 1; j < MAX_ENEMIES; j++)
        {
            Enemy *b = &list->items[j];
            if (!Enemy_IsAlive(b)) continue;

            float minDistance = radiusA + Enemy_GetDef(b->type)->radius;
            float distance = Util_FlatDistance(a->position, b->position);
            if ((distance >= minDistance) || (distance < 0.0001f)) continue;

            Vector3 push = Vector3Scale(Util_FlatDirection(b->position, a->position), (minDistance - distance)*0.5f);
            a->position = Vector3Add(a->position, push);
            b->position = Vector3Subtract(b->position, push);
        }

        if (player->isAlive)
        {
            float minDistance = radiusA + PLAYER_RADIUS;
            float distance = Util_FlatDistance(a->position, player->position);
            if ((distance < minDistance) && (distance > 0.0001f))
            {
                Vector3 push = Vector3Scale(Util_FlatDirection(player->position, a->position), minDistance - distance);
                a->position = Vector3Add(a->position, push);
            }
        }

        Level_ResolveCircle(level, &a->position, radiusA, NULL);
    }
}

void Enemies_Update(EnemyList *list, const Level *level, Player *player, ProjectilePool *projectiles, float deltaTime)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *enemy = &list->items[i];
        if (!enemy->active) continue;

        if (enemy->state == ENEMY_STATE_DEAD)
        {
            enemy->deathTimer += deltaTime;
            if (enemy->deathTimer > CORPSE_LIFETIME) enemy->active = false;
            continue;
        }

        UpdateEnemy(enemy, level, player, projectiles, deltaTime);
    }

    SeparateBodies(list, level, player);
}

//----------------------------------------------------------------------------------
// Drawing
//----------------------------------------------------------------------------------
static Color GetBodyColor(const Enemy *enemy, const EnemyDef *def)
{
    Color color = def->color;

    // Telegraph attacks by glowing toward yellow during the windup
    if ((enemy->state == ENEMY_STATE_ATTACK) && (def->attackWindup > 0.0f))
    {
        float windup = 1.0f - enemy->attackTimer/def->attackWindup;
        color = ColorLerp(color, (Color){ 255, 220, 80, 255 }, windup*0.7f);
    }

    if (enemy->hitFlash > 0.0f) color = ColorLerp(color, WHITE, enemy->hitFlash);
    return Render_ApplyFog(color, enemy->position);
}

static void DrawCorpse(const Enemy *enemy, const EnemyDef *def)
{
    float fall = Clamp(enemy->deathTimer/DEATH_FALL_TIME, 0.0f, 1.0f);
    Vector3 forward = Util_DirectionFromYaw(enemy->yaw);
    Vector3 base = Vector3Add(enemy->position, (Vector3){ 0.0f, def->radius*0.5f*fall, 0.0f });

    // Tip the body over backwards
    Vector3 upright = Vector3Add(enemy->position, (Vector3){ 0.0f, def->height*0.7f, 0.0f });
    Vector3 lying = Vector3Add(base, Vector3Scale(forward, -def->height*0.7f));
    Vector3 top = Vector3Lerp(upright, lying, fall);

    Color color = Render_ApplyFog(Render_Shade(def->color, 0.45f), enemy->position);
    DrawCylinderEx(base, top, def->radius*0.9f, def->radius*0.75f, 10, color);
    DrawSphereEx(Vector3Add(top, Vector3Scale(Vector3Normalize(Vector3Subtract(top, base)), def->radius*0.4f)), def->radius*0.5f, 8, 8, color);
}

static void DrawEnemy(const Enemy *enemy)
{
    const EnemyDef *def = Enemy_GetDef(enemy->type);

    if (enemy->state == ENEMY_STATE_DEAD)
    {
        DrawCorpse(enemy, def);
        return;
    }

    Color body = GetBodyColor(enemy, def);
    Color dark = Render_Shade(body, 0.6f);
    Vector3 forward = Util_DirectionFromYaw(enemy->yaw);
    Vector3 right = GetRightVector(enemy->yaw);
    float bob = fabsf(sinf(enemy->walkCycle))*0.06f;
    float windup = ((enemy->state == ENEMY_STATE_ATTACK) && (def->attackWindup > 0.0f)) ? 1.0f - enemy->attackTimer/def->attackWindup : 0.0f;

    // Torso + head
    Vector3 feet = enemy->position;
    Vector3 chest = Vector3Add(feet, (Vector3){ 0.0f, def->height*0.65f + bob, 0.0f });
    Vector3 head = Vector3Add(feet, (Vector3){ 0.0f, def->height*0.82f + bob, 0.0f });
    DrawCylinderEx(feet, chest, def->radius*0.75f, def->radius, 10, body);
    DrawSphereEx(head, def->radius*0.55f, 10, 10, body);

    // Eyes show facing direction; red once alerted
    bool alerted = (enemy->state == ENEMY_STATE_CHASE) || (enemy->state == ENEMY_STATE_ATTACK);
    Color eyeColor = alerted ? (Color){ 255, 40, 40, 255 } : (Color){ 255, 230, 120, 255 };
    Vector3 eyeCenter = Vector3Add(head, Vector3Add(Vector3Scale(forward, def->radius*0.45f), (Vector3){ 0.0f, def->radius*0.1f, 0.0f }));
    DrawSphereEx(Vector3Add(eyeCenter, Vector3Scale(right, def->radius*0.2f)), def->radius*0.1f, 6, 6, eyeColor);
    DrawSphereEx(Vector3Subtract(eyeCenter, Vector3Scale(right, def->radius*0.2f)), def->radius*0.1f, 6, 6, eyeColor);

    switch (def->attackKind)
    {
        case ATTACK_MELEE:
        {
            // Arms swing forward during the windup
            float reach = Lerp(0.35f, 1.1f, windup);
            for (int side = -1; side <= 1; side += 2)
            {
                Vector3 shoulder = Vector3Add(chest, Vector3Scale(right, def->radius*0.95f*(float)side));
                Vector3 hand = Vector3Add(shoulder, Vector3Add(Vector3Scale(forward, reach), (Vector3){ 0.0f, -0.45f + windup*0.4f, 0.0f }));
                DrawCylinderEx(shoulder, hand, def->radius*0.22f, def->radius*0.18f, 6, dark);
            }
        } break;
        case ATTACK_PROJECTILE:
        {
            Vector3 muzzle = GetMuzzlePosition(enemy);
            Vector3 gunBase = Vector3Subtract(muzzle, Vector3Scale(forward, 0.9f));
            DrawCylinderEx(gunBase, muzzle, 0.09f, 0.07f, 6, Render_ApplyFog(DARKGRAY, enemy->position));
            if (windup > 0.0f) DrawSphereEx(muzzle, 0.08f + windup*0.15f, 6, 6, (Color){ 255, 160, 60, 255 });
        } break;
        default: break;
    }
}

void Enemies_Draw(const EnemyList *list)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (list->items[i].active) DrawEnemy(&list->items[i]);
    }
}
