#pragma once

#include "raylib.h"
#include "flecs.h"
#include "enemies/EnemyDefs.h"
#include "items/PickupTypes.h"
#include "weapons/Weapon.h"

//----------------------------------------------------------------------------------
// ECS components: plain data, no logic. Systems (in the feature folders) do the work.
// An entity is just an id; what it "is" comes from which components it has, e.g.
//   enemy      = WorldTransform + Collider + Health + Enemy + EnemyBrain + EnemyVisual
//   corpse     = an enemy that also has Dead + Lifetime
//   projectile = WorldTransform + Velocity + Projectile + Lifetime
//   tracer     = Tracer + Lifetime
//   boss       = enemy components (minus EnemyBrain) + Boss + SkullLordAttacks / LichAttacks
//   crystal    = enemy components (minus EnemyBrain) + LichCrystal + Anchored
//   pickup     = WorldTransform + Pickup
//   switch     = WorldTransform + WallSwitch
//   hazard     = WorldTransform + DamageZone + Lifetime
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

struct Invulnerable {};         // Tag: damage is ignored (Lich while its crystals stand)
struct Anchored {};             // Tag: never pushed around by crowd separation (crystals)

// Bosses: Boss holds what every boss shares; each boss type adds its own attack component
enum class BossType
{
    SkullLord,
    Lich,
    Count
};

struct Boss
{
    BossType type;
    bool engaged;               // Wakes up once it sees the player or gets hurt
    bool canSeePlayer;
    float senseTimer;
    float hoverHeight;
    float strafeDirection;
    float strafeTimer;
    float bobPhase;
};

struct SkullLordAttacks
{
    float boltTimer;            // Next bolt volley
    float zoneTimer;            // Next set of damage zones
};

struct LichAttacks
{
    float summonTimer;          // Next enemy summon
    float zoneTimer;            // Next ring of damage zones
    float trailTimer;           // Next "zones under the player" barrage
    float trailRemaining;       // > 0 while a barrage is running
    float trailTick;            // Time to the next zone of the barrage
    bool crystalsSpawned;       // Half-health phase already triggered
    float castFlash;            // Visual: staff glow after casting
    float boltTimer;            // Next soul bolt burst
    int burstRemaining;         // Bolts left in the current burst
    float burstTick;            // Time to the next bolt of the burst
};

struct LichCrystal              // Keeps its Lich immune until destroyed
{
    flecs::entity_t owner;
    float spin;
};

// Combat
enum class Team
{
    Player,
    Enemy
};

enum class ProjectileShape
{
    Orb,
    Arrow
};

struct Projectile
{
    Team team;              // Projectiles never hurt their own team
    float damage;
    float radius;
    Color color;
    float gravity;          // Downward acceleration (0 = flies straight)
    ProjectileShape shape;
    float headshotMultiplier;
};

struct DamageZone               // Telegraphed area that hurts anyone of the other team standing in it
{
    Team team;
    float radius;
    float height;
    float damagePerSecond;
    float warmup;               // Seconds of warning before it starts hurting
    float warmupTotal;
    Color color;
};

// Items
struct Pickup
{
    PickupType type;
    WeaponType weapon;          // Ammo only
    int amount;                 // Ammo rounds / health points / boost seconds
    int link;                   // Key only
    float bobPhase;
};

struct WallSwitch
{
    int link;                   // Opens every door with the same link tag
    Vector3 normal;             // Facing out of the wall
    bool activated;
};

// Effects
struct Tracer
{
    Vector3 start;
    Vector3 end;
    Color color;
};

struct StuckArrow              // Arrow left in a wall
{
    Vector3 tip;
    Vector3 direction;
};

struct Impact
{
    Vector3 position;
    float size;
    Color color;
};
