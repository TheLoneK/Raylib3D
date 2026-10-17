#include "minimap.h"
#include "render.h"
#include "utils.h"
#include "raymath.h"

// Describes how world space maps onto the minimap rectangle for one frame
typedef struct MapView {
    Rectangle bounds;
    Vector2 center;     // Screen position of focus
    Vector3 focus;      // World position at the center
    float scale;        // Pixels per world unit
} MapView;

MinimapSettings Minimap_DefaultSettings(void)
{
    return (MinimapSettings){
        .compactSize = 200.0f,
        .compactWorldRadius = 40.0f,
        .margin = 16.0f,
        .showEnemies = true,
        .backgroundColor = (Color){ 10, 12, 18, 200 },
        .floorColor = (Color){ 70, 78, 90, 230 },
        .wallColor = (Color){ 150, 140, 165, 255 },
        .borderColor = (Color){ 200, 200, 210, 255 },
        .playerColor = (Color){ 80, 230, 120, 255 },
    };
}

void Minimap_Init(Minimap *minimap, MinimapSettings settings)
{
    minimap->settings = settings;
    minimap->expanded = false;
}

void Minimap_ToggleExpanded(Minimap *minimap)
{
    minimap->expanded = !minimap->expanded;
}

static MapView GetMapView(const Minimap *minimap, const Level *level, const Player *player)
{
    const MinimapSettings *s = &minimap->settings;
    float screenWidth = (float)GetScreenWidth();
    float screenHeight = (float)GetScreenHeight();
    MapView view = { 0 };

    if (minimap->expanded)
    {
        float size = fminf(screenWidth, screenHeight) - 120.0f;
        float worldSize = fmaxf((float)level->width, (float)level->depth)*level->tileSize;
        view.bounds = (Rectangle){ (screenWidth - size)*0.5f, (screenHeight - size)*0.5f, size, size };
        view.focus = Level_GetCenter(level);
        view.scale = size/worldSize;
    }
    else
    {
        view.bounds = (Rectangle){ screenWidth - s->compactSize - s->margin, s->margin, s->compactSize, s->compactSize };
        view.focus = player->position;
        view.scale = (s->compactSize*0.5f)/s->compactWorldRadius;
    }

    view.center = (Vector2){ view.bounds.x + view.bounds.width*0.5f, view.bounds.y + view.bounds.height*0.5f };
    return view;
}

// North-up mapping: world X -> screen X, world Z -> screen Y
static Vector2 WorldToMap(const MapView *view, Vector3 world)
{
    return (Vector2){
        view->center.x + (world.x - view->focus.x)*view->scale,
        view->center.y + (world.z - view->focus.z)*view->scale,
    };
}

static void DrawTiles(const MapView *view, const Level *level, const MinimapSettings *s)
{
    float cellPixels = level->tileSize*view->scale;

    for (int z = 0; z < level->depth; z++)
    {
        for (int x = 0; x < level->width; x++)
        {
            Vector2 topLeft = WorldToMap(view, (Vector3){ (float)x*level->tileSize, 0.0f, (float)z*level->tileSize });
            Rectangle cell = (Rectangle){ topLeft.x, topLeft.y, cellPixels + 0.5f, cellPixels + 0.5f };
            if (!CheckCollisionRecs(cell, view->bounds)) continue;

            if (!Level_IsSolid(level, x, z))
            {
                DrawRectangleRec(cell, s->floorColor);
            }
            else if (!Level_IsSolid(level, x + 1, z) || !Level_IsSolid(level, x - 1, z) ||
                     !Level_IsSolid(level, x, z + 1) || !Level_IsSolid(level, x, z - 1))
            {
                DrawRectangleRec(cell, s->wallColor);   // Only outline walls that border open space
            }
        }
    }
}

static void DrawEnemies(const MapView *view, const EnemyList *enemies)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        const Enemy *enemy = &enemies->items[i];
        if (!Enemy_IsAlive(enemy)) continue;

        Vector2 position = WorldToMap(view, enemy->position);
        Vector3 facing = Util_DirectionFromYaw(enemy->yaw);
        Color color = Enemy_GetDef(enemy->type)->color;

        DrawLineEx(position, (Vector2){ position.x + facing.x*8.0f, position.y + facing.z*8.0f }, 2.0f, color);
        DrawCircleV(position, 4.0f, color);
        if ((enemy->state == ENEMY_STATE_CHASE) || (enemy->state == ENEMY_STATE_ATTACK)) DrawCircleLinesV(position, 6.0f, RED);
    }
}

static void DrawPlayerArrow(const MapView *view, const Player *player, Color color)
{
    Vector2 position = WorldToMap(view, player->position);
    Vector3 facing3 = Util_DirectionFromYaw(player->yaw);
    Vector2 forward = (Vector2){ facing3.x, facing3.z };
    Vector2 side = (Vector2){ -forward.y, forward.x };

    Vector2 tip = Vector2Add(position, Vector2Scale(forward, 9.0f));
    Vector2 left = Vector2Add(Vector2Subtract(position, Vector2Scale(forward, 6.0f)), Vector2Scale(side, 6.0f));
    Vector2 right = Vector2Subtract(Vector2Subtract(position, Vector2Scale(forward, 6.0f)), Vector2Scale(side, 6.0f));

    Render_DrawTriangle2D(tip, left, right, color);
}

void Minimap_Draw(const Minimap *minimap, const Level *level, const Player *player, const EnemyList *enemies)
{
    const MinimapSettings *s = &minimap->settings;
    MapView view = GetMapView(minimap, level, player);

    DrawRectangleRec(view.bounds, s->backgroundColor);

    BeginScissorMode((int)view.bounds.x, (int)view.bounds.y, (int)view.bounds.width, (int)view.bounds.height);
        DrawTiles(&view, level, s);
        if (s->showEnemies) DrawEnemies(&view, enemies);
        DrawPlayerArrow(&view, player, s->playerColor);
    EndScissorMode();

    DrawRectangleLinesEx(view.bounds, 2.0f, s->borderColor);
    DrawText(minimap->expanded ? "MAP  [M] close" : "MAP [M]", (int)view.bounds.x + 6, (int)(view.bounds.y + view.bounds.height) + 4, 10, s->borderColor);
}
