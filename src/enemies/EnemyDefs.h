#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Enemy definition table. Each enemy entity stores only its EnemyType; the stats
// are looked up here so tuning happens in one place.
//----------------------------------------------------------------------------------
// To add an enemy: add a type here and fill its definition in EnemyDefs.cpp
enum class EnemyType
{
    Brute = 0,      // Melee
    Gunner,         // Ranged
    SkullBoss,      // Boss: has its own systems (Boss.cpp) instead of EnemyBrain
    Count
};

enum class AttackKind
{
    Melee,
    Projectile
};

struct EnemyDef
{
    const char *name = "";
    AttackKind attackKind = AttackKind::Melee;
    float maxHealth = 100.0f;
    float radius = 0.5f;
    float height = 1.8f;
    float wanderSpeed = 2.0f;
    float chaseSpeed = 5.0f;
    float turnSpeed = 6.0f;             // Radians per second
    float sightRange = 40.0f;
    float sightFovDegrees = 120.0f;     // Only used while unaware; alerted enemies see all around
    float memoryTime = 5.0f;            // Seconds they keep hunting after losing sight
    float attackRange = 2.0f;           // Starts an attack inside this distance
    float preferredRange = 0.0f;        // Ranged enemies hold this distance (0 = always close in)
    float attackDamage = 10.0f;
    float attackWindup = 0.3f;          // Telegraph before the hit/shot lands
    float attackCooldown = 1.0f;
    float projectileSpeed = 0.0f;
    float projectileRadius = 0.0f;
    float aimSpreadDegrees = 0.0f;
    Color color = { 255, 255, 255, 255 };
};

const EnemyDef &GetEnemyDef(EnemyType type);
float GetLargestEnemyRadius();
