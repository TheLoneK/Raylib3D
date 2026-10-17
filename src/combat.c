#include "combat.h"
#include "utils.h"
#include "raymath.h"
#include <stddef.h>

static const Color BLOOD_COLOR = { 200, 30, 30, 255 };
static const Color SPARK_COLOR = { 255, 220, 140, 255 };

// Closest living enemy hit by the ray within maxDistance (NULL if none)
static Enemy *RaycastEnemies(EnemyList *enemies, Ray ray, float maxDistance, float padding, float *outDistance)
{
    Enemy *closest = NULL;
    float best = maxDistance;

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *enemy = &enemies->items[i];
        if (!Enemy_IsAlive(enemy)) continue;

        BoundingBox bounds = Enemy_GetBounds(enemy);
        bounds.min = Vector3Subtract(bounds.min, (Vector3){ padding, padding, padding });
        bounds.max = Vector3Add(bounds.max, (Vector3){ padding, padding, padding });

        RayCollision hit = GetRayCollisionBox(ray, bounds);
        if (hit.hit && (hit.distance <= best))
        {
            best = hit.distance;
            closest = enemy;
        }
    }

    if (outDistance != NULL) *outDistance = best;
    return closest;
}

static void ApplyEnemyHit(CombatResult *result, Enemy *enemy, float damage, Vector3 sourcePosition)
{
    result->hits++;
    if (Enemy_TakeDamage(enemy, damage, sourcePosition)) result->kills++;
}

//----------------------------------------------------------------------------------
// Player weapon
//----------------------------------------------------------------------------------
static Vector3 GetViewmodelMuzzle(const Player *player)
{
    // Roughly where the on-screen barrel ends, so tracers look like they leave the gun
    Vector3 eye = Player_GetEyePosition(player);
    Vector3 forward = Player_GetForward(player);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, (Vector3){ 0.0f, 1.0f, 0.0f }));
    Vector3 up = Vector3CrossProduct(right, forward);

    Vector3 muzzle = Vector3Add(eye, Vector3Scale(forward, 0.6f));
    muzzle = Vector3Add(muzzle, Vector3Scale(right, 0.12f));
    return Vector3Add(muzzle, Vector3Scale(up, -0.12f));
}

static void FireHitscan(CombatWorld world, const WeaponDef *def, Vector3 origin, Vector3 direction, CombatResult *result)
{
    Ray ray = (Ray){ .position = origin, .direction = direction };
    RayCollision wallHit = Level_Raycast(world.level, ray, def->range);

    float enemyDistance = 0.0f;
    Enemy *target = RaycastEnemies(world.enemies, ray, wallHit.distance, 0.0f, &enemyDistance);
    float distance = (target != NULL) ? enemyDistance : wallHit.distance;
    Vector3 end = Vector3Add(origin, Vector3Scale(direction, distance));

    if (target != NULL)
    {
        ApplyEnemyHit(result, target, def->damage, world.player->position);
        Effects_AddImpact(world.effects, end, 0.12f, BLOOD_COLOR);
    }
    else if (wallHit.hit)
    {
        Effects_AddImpact(world.effects, end, 0.08f, SPARK_COLOR);
    }

    Effects_AddTracer(world.effects, GetViewmodelMuzzle(world.player), end, ColorLerp(def->color, SPARK_COLOR, 0.6f));
}

static void FireProjectile(CombatWorld world, const WeaponDef *def, Vector3 origin, Vector3 direction)
{
    // Spawn from the eye so projectiles always travel toward the crosshair
    Vector3 start = Vector3Add(origin, Vector3Scale(direction, 0.5f));
    float lifetime = def->range/fmaxf(def->projectileSpeed, 0.01f);
    Projectiles_Spawn(world.projectiles, TEAM_PLAYER, start, Vector3Scale(direction, def->projectileSpeed),
                      def->projectileRadius, def->damage, lifetime, def->color);
}

CombatResult Combat_UpdatePlayerWeapon(CombatWorld world, const PlayerInput *input)
{
    CombatResult result = { 0 };
    Player *player = world.player;
    if (!player->isAlive || Inventory_IsSwitching(&player->inventory)) return result;

    Weapon *weapon = Inventory_GetActiveWeapon(&player->inventory);
    if ((weapon == NULL) || !Weapon_TryFire(weapon, input->firePressed, input->fireHeld)) return result;

    const WeaponDef *def = Weapon_GetDef(weapon->type);
    Vector3 eye = Player_GetEyePosition(player);
    Vector3 forward = Player_GetForward(player);
    result.shotsFired = 1;

    for (int pellet = 0; pellet < def->pelletsPerShot; pellet++)
    {
        Vector3 direction = Util_RandomConeDirection(forward, def->spreadDegrees);

        switch (def->shotKind)
        {
            case SHOT_HITSCAN: FireHitscan(world, def, eye, direction, &result); break;
            case SHOT_PROJECTILE: FireProjectile(world, def, eye, direction); break;
            default: break;
        }
    }

    Enemies_AlertNoise(world.enemies, player->position, def->noiseRadius);
    return result;
}

//----------------------------------------------------------------------------------
// Projectiles
//----------------------------------------------------------------------------------
CombatResult Combat_UpdateProjectiles(CombatWorld world, float deltaTime)
{
    CombatResult result = { 0 };

    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *projectile = &world.projectiles->items[i];
        if (!projectile->active) continue;

        projectile->lifetime -= deltaTime;
        if (projectile->lifetime <= 0.0f)
        {
            projectile->active = false;
            continue;
        }

        // Sweep along this frame's movement so fast projectiles cannot tunnel
        Vector3 step = Vector3Scale(projectile->velocity, deltaTime);
        float stepLength = Vector3Length(step);
        if (stepLength < 0.00001f) continue;

        Ray ray = (Ray){ .position = projectile->position, .direction = Vector3Scale(step, 1.0f/stepLength) };
        RayCollision wallHit = Level_Raycast(world.level, ray, stepLength + projectile->radius);
        float travel = wallHit.hit ? wallHit.distance : stepLength;
        Vector3 hitPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, travel));
        bool consumed = wallHit.hit;

        if (projectile->team == TEAM_ENEMY)
        {
            Player *player = world.player;
            if (player->isAlive)
            {
                BoundingBox bounds = Player_GetBounds(player);
                bounds.min = Vector3Subtract(bounds.min, (Vector3){ projectile->radius, projectile->radius, projectile->radius });
                bounds.max = Vector3Add(bounds.max, (Vector3){ projectile->radius, projectile->radius, projectile->radius });

                RayCollision playerHit = GetRayCollisionBox(ray, bounds);
                if (playerHit.hit && (playerHit.distance <= travel))
                {
                    Player_TakeDamage(player, projectile->damage);
                    hitPoint = playerHit.point;
                    consumed = true;
                }
            }
        }
        else
        {
            float enemyDistance = 0.0f;
            Enemy *target = RaycastEnemies(world.enemies, ray, travel, projectile->radius, &enemyDistance);
            if (target != NULL)
            {
                ApplyEnemyHit(&result, target, projectile->damage, world.player->position);
                hitPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, enemyDistance));
                consumed = true;
            }
        }

        if (consumed)
        {
            Effects_AddImpact(world.effects, hitPoint, projectile->radius*1.5f, projectile->color);
            projectile->active = false;
        }
        else
        {
            projectile->position = hitPoint;
        }
    }

    return result;
}
