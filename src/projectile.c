#include "projectile.h"
#include "render.h"
#include "raymath.h"
#include <stddef.h>

void Projectiles_Clear(ProjectilePool *pool)
{
    *pool = (ProjectilePool){ 0 };
}

Projectile *Projectiles_Spawn(ProjectilePool *pool, Team team, Vector3 position, Vector3 velocity, float radius, float damage, float lifetime, Color color)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        Projectile *projectile = &pool->items[i];
        if (projectile->active) continue;

        *projectile = (Projectile){
            .active = true,
            .team = team,
            .position = position,
            .velocity = velocity,
            .radius = radius,
            .damage = damage,
            .lifetime = lifetime,
            .color = color,
        };
        return projectile;
    }

    return NULL;    // Pool is full
}

void Projectiles_Draw(const ProjectilePool *pool)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        const Projectile *projectile = &pool->items[i];
        if (!projectile->active) continue;

        // Short trail behind the core makes fast projectiles readable
        Vector3 tail = Vector3Subtract(projectile->position, Vector3Scale(projectile->velocity, 0.03f));
        DrawCylinderEx(tail, projectile->position, projectile->radius*0.3f, projectile->radius*0.8f, 6, Fade(projectile->color, 0.6f));
        DrawSphereEx(projectile->position, projectile->radius, 8, 8, projectile->color);
    }
}
