#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "level.h"
#include "player.h"
#include "enemy.h"
#include "projectile.h"
#include "effects.h"
#include "minimap.h"
#include "hud.h"

//----------------------------------------------------------------------------------
// Game: owns every system and wires them together each frame
//----------------------------------------------------------------------------------
typedef struct Game {
    Level level;
    Player player;
    EnemyList enemies;
    ProjectilePool projectiles;
    EffectPool effects;
    Minimap minimap;
    HudState hud;
    Camera3D camera;
    unsigned int seed;
} Game;

void Game_Init(Game *game, unsigned int seed);          // First time setup
void Game_LoadLevel(Game *game, unsigned int seed);     // (Re)generate the level and respawn everything
void Game_Update(Game *game, float deltaTime);
void Game_Draw(const Game *game);

#endif // GAME_H
