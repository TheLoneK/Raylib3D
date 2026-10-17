#ifndef ENEMY_H
#define ENEMY_H

#include "raylib.h"
#include "level.h"
#include "player.h"
#include "projectile.h"

//----------------------------------------------------------------------------------
// Enemies: definition table + simple state machine AI
//----------------------------------------------------------------------------------
#define MAX_ENEMIES     32

// To add an enemy: add a type here and fill its definition in enemy.c
typedef enum EnemyType {
    ENEMY_TYPE_BRUTE = 0,       // Melee
    ENEMY_TYPE_GUNNER,          // Ranged
    ENEMY_TYPE_COUNT
} EnemyType;

typedef enum AttackKind {
    ATTACK_MELEE = 0,
    ATTACK_PROJECTILE
} AttackKind;

typedef enum EnemyState {
    ENEMY_STATE_IDLE = 0,       // Standing around, then picks a wander target
    ENEMY_STATE_WANDER,         // Walking to a random nearby point
    ENEMY_STATE_CHASE,          // Knows where the player is (or was)
    ENEMY_STATE_ATTACK,         // Winding up an attack
    ENEMY_STATE_DEAD
} EnemyState;

typedef struct EnemyDef {
    const char *name;
    AttackKind attackKind;
    float maxHealth;
    float radius;
    float height;
    float wanderSpeed;
    float chaseSpeed;
    float turnSpeed;            // Radians per second
    float sightRange;
    float sightFovDegrees;      // Only used while unaware; alerted enemies see all around
    float memoryTime;           // Seconds they keep hunting after losing sight
    float attackRange;          // Starts an attack inside this distance
    float preferredRange;       // Ranged enemies hold this distance (0 = always close in)
    float attackDamage;
    float attackWindup;         // Telegraph before the hit/shot lands
    float attackCooldown;
    float projectileSpeed;
    float projectileRadius;
    float aimSpreadDegrees;
    Color color;
} EnemyDef;

typedef struct Enemy {
    bool active;
    EnemyType type;
    EnemyState state;
    Vector3 position;           // Feet
    float yaw;
    float health;

    Vector3 moveTarget;
    Vector3 lastKnownPlayerPosition;
    bool canSeePlayer;

    float stateTimer;           // Generic countdown for the current state
    float attackTimer;          // Windup countdown
    float cooldownTimer;
    float loseSightTimer;
    float hitFlash;
    float deathTimer;
    float strafeDirection;
    float strafeTimer;
    float walkCycle;

    Vector3 stuckCheckPosition;
    float stuckCheckTimer;
} Enemy;

typedef struct EnemyList {
    Enemy items[MAX_ENEMIES];
} EnemyList;

const EnemyDef *Enemy_GetDef(EnemyType type);

void Enemies_Clear(EnemyList *list);
Enemy *Enemies_Spawn(EnemyList *list, EnemyType type, Vector3 position, float yaw);
void Enemies_Update(EnemyList *list, const Level *level, Player *player, ProjectilePool *projectiles, float deltaTime);
void Enemies_Draw(const EnemyList *list);
void Enemies_AlertNoise(EnemyList *list, Vector3 noisePosition, float radius);
int Enemies_CountAlive(const EnemyList *list);

bool Enemy_IsAlive(const Enemy *enemy);
bool Enemy_TakeDamage(Enemy *enemy, float amount, Vector3 sourcePosition);   // Returns true if this killed it
BoundingBox Enemy_GetBounds(const Enemy *enemy);
Vector3 Enemy_GetEyePosition(const Enemy *enemy);
Vector3 Enemy_GetHeadTop(const Enemy *enemy);

#endif // ENEMY_H
