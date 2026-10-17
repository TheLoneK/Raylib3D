#ifndef HUD_H
#define HUD_H

#include "raylib.h"
#include "player.h"
#include "enemy.h"
#include "level.h"
#include "minimap.h"

//----------------------------------------------------------------------------------
// 2D overlay: viewmodel, crosshair, health, ammo, inventory bar, minimap
//----------------------------------------------------------------------------------
typedef struct HudState {
    float hitMarkerTimer;
    float killMarkerTimer;
    bool showHelp;
} HudState;

// Everything the HUD reads in one frame
typedef struct HudContext {
    const Player *player;
    const EnemyList *enemies;
    const Level *level;
    const Minimap *minimap;
    Camera3D camera;
    unsigned int seed;
} HudContext;

void Hud_Init(HudState *hud);
void Hud_Update(HudState *hud, float deltaTime);
void Hud_RegisterHits(HudState *hud, int hits, int kills);
void Hud_Draw(const HudState *hud, HudContext context);

#endif // HUD_H
