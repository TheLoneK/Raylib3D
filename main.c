/*******************************************************************************************
*
*   Raylib3D - simple first person shooter framework
*
*   main.c only owns the window and the main loop; all game code lives in src/
*
*   src/level      procedural rooms + corridors, wall collision, raycasts
*   src/player     movement (based on raylib's "3d camera fps" example), health, inventory
*   src/weapon     weapon definition table, ammo/magazine/reload state
*   src/inventory  7 weapon slots, switching
*   src/enemy      enemy definition table, wander/chase/attack AI
*   src/combat     player shooting, projectile movement and hits
*   src/projectile projectile pool,  src/effects  tracers/impacts
*   src/minimap    corner + expanded map,  src/hud  2D overlay
*   src/game       owns every system and runs a frame
*
********************************************************************************************/

#include "raylib.h"
#include "config.h"
#include "game.h"

#include <time.h>

static Game game = { 0 };   // Large, so it lives outside the stack

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(TARGET_FPS);
    DisableCursor();        // Limit cursor to relative movement inside the window

    Game_Init(&game, (unsigned int)time(NULL));

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        Game_Update(&game, GetFrameTime());
        Game_Draw(&game);
    }

    CloseWindow();
    return 0;
}
