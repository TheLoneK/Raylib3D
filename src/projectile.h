#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "raylib.h"

//----------------------------------------------------------------------------------
// Projectile pool (storage + drawing). Movement and hits live in combat.c
//----------------------------------------------------------------------------------
#define MAX_PROJECTILES     128

typedef enum Team {
    TEAM_PLAYER = 0,
    TEAM_ENEMY
} Team;

typedef struct Projectile {
    bool active;
    Team team;              // Projectiles never hurt their own team
    Vector3 position;
    Vector3 velocity;
    float radius;
    float damage;
    float lifetime;         // Seconds left before it fizzles out
    Color color;
} Projectile;

typedef struct ProjectilePool {
    Projectile items[MAX_PROJECTILES];
} ProjectilePool;

void Projectiles_Clear(ProjectilePool *pool);
Projectile *Projectiles_Spawn(ProjectilePool *pool, Team team, Vector3 position, Vector3 velocity, float radius, float damage, float lifetime, Color color);
void Projectiles_Draw(const ProjectilePool *pool);

#endif // PROJECTILE_H
