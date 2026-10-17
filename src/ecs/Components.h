#pragma once

#include "raylib.h"
#include "enemies/EnemyDefs.h"

//----------------------------------------------------------------------------------
// ECS components: plain data, no logic. Systems (in the feature folders) do the work.
// An entity is just an id; what it "is" comes from which components it has, e.g.
//   enemy      = WorldTransform + Collider + Health + Enemy + EnemyBrain + EnemyVisual
//   corpse     = an enemy that also has Dead + Lifetime
//   projectile = WorldTransform + Velocity + Projectile + Lifetime
//   tracer     = Tracer + Lifetime
//----------------------------------------------------------------------------------

// Shared
struct WorldTransform           // (raylib already has a "Transform" type)
{
    Vector3 position;       // Feet / center for small objects
    float yaw;
};

struct Velocity
{
    Vector3 value;
};

struct Collider
{
    float radius;
    float height;
};

struct Health
{
    float current;
    float max;
};

struct Lifetime                 // Entity is destroyed when it runs out
{
    float remaining;
    float total;
};

struct Dead {};                 // Tag: killed, kept around as a corpse

// Enemies
struct Enemy
{
    EnemyType type;
};

enum class EnemyState
{
    Idle,           // Standing around, then picks a wander target
    Wander,         // Walking to a random nearby point
    Chase,          // Hunting the player along the flow field
    Attack          // Winding up an attack
};

struct EnemyBrain
{
    EnemyState state;
    bool huntsPlayer;           // Always knows where the player is (no sight needed)
    bool canSeePlayer;          // Refreshed a few times per second, see senseTimer
    float senseTimer;
    float stateTimer;           // Generic countdown for the current state
    float attackTimer;          // Windup countdown
    float cooldownTimer;
    float loseSightTimer;
    float strafeDirection;
    float strafeTimer;
    Vector3 moveTarget;
    Vector3 stuckCheckPosition;
    float stuckCheckTimer;
};

struct EnemyVisual
{
    float hitFlash;
    float walkCycle;
};

// Combat
enum class Team
{
    Player,
    Enemy
};

struct Projectile
{
    Team team;              // Projectiles never hurt their own team
    float damage;
    float radius;
    Color color;
};

// Effects
struct Tracer
{
    Vector3 start;
    Vector3 end;
    Color color;
};

struct Impact
{
    Vector3 position;
    float size;
    Color color;
};
