#ifndef COMBAT_H
#define COMBAT_H

#include "level.h"
#include "player.h"
#include "enemy.h"
#include "projectile.h"
#include "effects.h"

//----------------------------------------------------------------------------------
// Combat rules: player shooting, projectile movement and hits
//----------------------------------------------------------------------------------
typedef struct CombatResult {
    int shotsFired;
    int hits;           // Enemies hit by the player
    int kills;          // Enemies killed by the player
} CombatResult;

// Everything combat needs to touch in one place
typedef struct CombatWorld {
    const Level *level;
    Player *player;
    EnemyList *enemies;
    ProjectilePool *projectiles;
    EffectPool *effects;
} CombatWorld;

CombatResult Combat_UpdatePlayerWeapon(CombatWorld world, const PlayerInput *input);
CombatResult Combat_UpdateProjectiles(CombatWorld world, float deltaTime);

#endif // COMBAT_H
