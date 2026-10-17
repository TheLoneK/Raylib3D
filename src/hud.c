#include "hud.h"
#include "render.h"
#include "raymath.h"
#include <stddef.h>

static const Color HUD_TEXT = { 235, 235, 240, 255 };
static const Color HUD_PANEL = { 0, 0, 0, 140 };
static const Color HUD_ACCENT = { 255, 200, 80, 255 };

void Hud_Init(HudState *hud)
{
    *hud = (HudState){ .showHelp = true };
}

void Hud_Update(HudState *hud, float deltaTime)
{
    hud->hitMarkerTimer = fmaxf(0.0f, hud->hitMarkerTimer - deltaTime);
    hud->killMarkerTimer = fmaxf(0.0f, hud->killMarkerTimer - deltaTime);
}

void Hud_RegisterHits(HudState *hud, int hits, int kills)
{
    if (hits > 0) hud->hitMarkerTimer = 0.15f;
    if (kills > 0) hud->killMarkerTimer = 0.4f;
}

//----------------------------------------------------------------------------------
// Viewmodel (procedural 2D weapon drawn over the 3D view)
//----------------------------------------------------------------------------------
static void DrawQuad2D(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color)
{
    Render_DrawTriangle2D(a, b, c, color);
    Render_DrawTriangle2D(a, c, d, color);
}

static void DrawViewModel(const Player *player)
{
    const Weapon *weapon = Inventory_GetActiveWeaponConst(&player->inventory);
    if ((weapon == NULL) || !player->isAlive) return;

    const WeaponDef *def = Weapon_GetDef(weapon->type);
    float screenWidth = (float)GetScreenWidth();
    float screenHeight = (float)GetScreenHeight();

    // Offsets: switching lowers the gun, reloading dips it, walking bobs it, firing kicks it
    float lower = Inventory_GetLowerAmount(&player->inventory);
    float reloadDip = sinf(Weapon_GetReloadProgress(weapon)*PI);
    float walk = player->isGrounded ? Clamp(Player_GetHorizontalSpeed(player)/20.0f, 0.0f, 1.0f) : 0.0f;
    float time = (float)GetTime();

    Vector2 offset = (Vector2){
        sinf(time*8.0f)*10.0f*walk + weapon->recoil*6.0f,
        lower*screenHeight*0.45f + reloadDip*screenHeight*0.22f + fabsf(cosf(time*8.0f))*8.0f*walk + weapon->recoil*18.0f,
    };

    Vector2 base = (Vector2){ screenWidth*0.70f + offset.x, screenHeight + 20.0f + offset.y };
    Vector2 tip = (Vector2){ screenWidth*0.60f + offset.x, screenHeight*(1.0f - def->viewLength) + offset.y };

    Vector2 direction = Vector2Normalize(Vector2Subtract(tip, base));
    Vector2 side = (Vector2){ -direction.y, direction.x };
    float baseHalf = def->viewWidth;
    float tipHalf = def->viewWidth*0.45f;

    Vector2 baseRight = Vector2Add(base, Vector2Scale(side, baseHalf));
    Vector2 baseLeft = Vector2Subtract(base, Vector2Scale(side, baseHalf));
    Vector2 tipRight = Vector2Add(tip, Vector2Scale(side, tipHalf));
    Vector2 tipLeft = Vector2Subtract(tip, Vector2Scale(side, tipHalf));
    Color outline = Fade(BLACK, 0.6f);

    // Body, lit left face, outline
    DrawQuad2D(baseRight, baseLeft, tipLeft, tipRight, Render_Shade(def->color, 0.5f));
    DrawQuad2D(baseLeft, Vector2Subtract(base, Vector2Scale(side, baseHalf*0.35f)),
               Vector2Subtract(tip, Vector2Scale(side, tipHalf*0.35f)), tipLeft, def->color);
    DrawLineEx(baseRight, tipRight, 2.0f, outline);
    DrawLineEx(baseLeft, tipLeft, 2.0f, outline);
    DrawLineEx(tipLeft, tipRight, 2.0f, outline);

    // Muzzle opening
    DrawCircleV(tip, tipHalf*0.55f, Render_Shade(def->color, 0.25f));
    DrawCircleV(tip, tipHalf*0.3f, (Color){ 10, 10, 12, 255 });

    // Hand gripping the side
    Vector2 hand = Vector2Add(Vector2Add(base, Vector2Scale(direction, 90.0f)), Vector2Scale(side, baseHalf*0.3f));
    DrawCircleV(Vector2Add(hand, Vector2Scale(direction, -60.0f)), baseHalf*0.85f, (Color){ 60, 70, 60, 255 });  // Sleeve
    DrawCircleV(hand, baseHalf*0.6f, (Color){ 205, 160, 130, 255 });

    if (weapon->muzzleFlash > 0.0f)
    {
        float flash = weapon->muzzleFlash/0.05f;
        DrawCircleV(tip, 30.0f*flash + 12.0f, Fade((Color){ 255, 200, 80, 255 }, 0.8f));
        DrawCircleV(tip, 12.0f*flash + 4.0f, (Color){ 255, 250, 220, 255 });
    }
}

//----------------------------------------------------------------------------------
// Crosshair / markers
//----------------------------------------------------------------------------------
static void DrawCrosshair(const HudState *hud, const Player *player)
{
    Vector2 center = (Vector2){ (float)GetScreenWidth()*0.5f, (float)GetScreenHeight()*0.5f };
    const Weapon *weapon = Inventory_GetActiveWeaponConst(&player->inventory);
    float spread = (weapon != NULL) ? Weapon_GetDef(weapon->type)->spreadDegrees : 0.0f;
    float recoil = (weapon != NULL) ? weapon->recoil : 0.0f;
    float gap = 5.0f + spread*2.0f + recoil*8.0f;
    float length = 7.0f;

    DrawLineEx((Vector2){ center.x - gap - length, center.y }, (Vector2){ center.x - gap, center.y }, 2.0f, HUD_TEXT);
    DrawLineEx((Vector2){ center.x + gap, center.y }, (Vector2){ center.x + gap + length, center.y }, 2.0f, HUD_TEXT);
    DrawLineEx((Vector2){ center.x, center.y - gap - length }, (Vector2){ center.x, center.y - gap }, 2.0f, HUD_TEXT);
    DrawLineEx((Vector2){ center.x, center.y + gap }, (Vector2){ center.x, center.y + gap + length }, 2.0f, HUD_TEXT);
    DrawCircleV(center, 1.5f, HUD_TEXT);

    if ((hud->hitMarkerTimer > 0.0f) || (hud->killMarkerTimer > 0.0f))
    {
        Color color = (hud->killMarkerTimer > 0.0f) ? RED : WHITE;
        float inner = 6.0f, outer = 13.0f;
        for (int sx = -1; sx <= 1; sx += 2)
        {
            for (int sy = -1; sy <= 1; sy += 2)
            {
                DrawLineEx((Vector2){ center.x + inner*(float)sx, center.y + inner*(float)sy },
                           (Vector2){ center.x + outer*(float)sx, center.y + outer*(float)sy }, 2.0f, color);
            }
        }
    }
}

//----------------------------------------------------------------------------------
// Status panels
//----------------------------------------------------------------------------------
static void DrawHealth(const Player *player)
{
    int screenHeight = GetScreenHeight();
    Rectangle panel = (Rectangle){ 20.0f, (float)screenHeight - 70.0f, 260.0f, 50.0f };
    float fraction = Clamp(player->health/player->maxHealth, 0.0f, 1.0f);
    Color barColor = (fraction > 0.5f) ? (Color){ 80, 200, 110, 255 } : ((fraction > 0.25f) ? HUD_ACCENT : RED);

    DrawRectangleRec(panel, HUD_PANEL);
    DrawText("HEALTH", (int)panel.x + 10, (int)panel.y + 6, 10, HUD_TEXT);
    DrawRectangle((int)panel.x + 10, (int)panel.y + 22, 180, 18, Fade(BLACK, 0.6f));
    DrawRectangle((int)panel.x + 10, (int)panel.y + 22, (int)(180.0f*fraction), 18, barColor);
    DrawText(TextFormat("%3d", (int)ceilf(player->health)), (int)panel.x + 200, (int)panel.y + 18, 24, HUD_TEXT);
}

static void DrawAmmo(const Player *player)
{
    const Weapon *weapon = Inventory_GetActiveWeaponConst(&player->inventory);
    if (weapon == NULL) return;

    const WeaponDef *def = Weapon_GetDef(weapon->type);
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    Rectangle panel = (Rectangle){ (float)screenWidth - 220.0f, (float)screenHeight - 70.0f, 200.0f, 50.0f };

    const char *reserve = Weapon_HasInfiniteAmmo(weapon) ? "INF" : TextFormat("%d", weapon->reserveAmmo);
    bool lowAmmo = (weapon->ammoInMagazine <= def->magazineSize/4);

    DrawRectangleRec(panel, HUD_PANEL);
    DrawText(def->name, (int)panel.x + 10, (int)panel.y + 6, 10, HUD_TEXT);
    DrawText(TextFormat("%d", weapon->ammoInMagazine), (int)panel.x + 10, (int)panel.y + 18, 28, lowAmmo ? RED : HUD_TEXT);
    DrawText(TextFormat("/ %s", reserve), (int)panel.x + 70, (int)panel.y + 26, 20, Fade(HUD_TEXT, 0.7f));

    if (Weapon_IsReloading(weapon))
    {
        Vector2 center = (Vector2){ (float)screenWidth*0.5f, (float)screenHeight*0.5f };
        DrawText("RELOADING", (int)center.x - MeasureText("RELOADING", 10)/2, (int)center.y + 30, 10, HUD_ACCENT);
        DrawRectangle((int)center.x - 40, (int)center.y + 44, 80, 4, Fade(BLACK, 0.6f));
        DrawRectangle((int)center.x - 40, (int)center.y + 44, (int)(80.0f*Weapon_GetReloadProgress(weapon)), 4, HUD_ACCENT);
    }
    else if (weapon->ammoInMagazine == 0)
    {
        const char *text = Weapon_HasInfiniteAmmo(weapon) || (weapon->reserveAmmo > 0) ? "RELOAD [R]" : "NO AMMO";
        DrawText(text, screenWidth/2 - MeasureText(text, 10)/2, screenHeight/2 + 30, 10, RED);
    }
}

static void DrawInventoryBar(const Player *player)
{
    const Inventory *inventory = &player->inventory;
    const float slotSize = 62.0f;
    const float gap = 6.0f;
    float totalWidth = (float)INVENTORY_SLOT_COUNT*slotSize + (float)(INVENTORY_SLOT_COUNT - 1)*gap;
    float startX = ((float)GetScreenWidth() - totalWidth)*0.5f;
    float y = (float)GetScreenHeight() - slotSize - 14.0f;
    int selected = Inventory_IsSwitching(inventory) ? inventory->pendingSlot : inventory->activeSlot;

    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++)
    {
        Rectangle slot = (Rectangle){ startX + (float)i*(slotSize + gap), y, slotSize, slotSize };
        const Weapon *weapon = &inventory->slots[i];
        bool empty = (weapon->type == WEAPON_NONE);
        bool isSelected = (i == selected);

        DrawRectangleRec(slot, isSelected ? Fade(HUD_ACCENT, 0.35f) : HUD_PANEL);
        DrawRectangleLinesEx(slot, isSelected ? 2.0f : 1.0f, isSelected ? HUD_ACCENT : Fade(HUD_TEXT, 0.3f));
        DrawText(TextFormat("%d", i + 1), (int)slot.x + 4, (int)slot.y + 3, 10, Fade(HUD_TEXT, 0.8f));

        if (empty) continue;

        const WeaponDef *def = Weapon_GetDef(weapon->type);
        DrawRectangle((int)slot.x + 8, (int)slot.y + 18, (int)slotSize - 16, 6, def->color);
        DrawText(def->shortName, (int)(slot.x + slotSize*0.5f) - MeasureText(def->shortName, 10)/2, (int)slot.y + 30, 10, HUD_TEXT);
        const char *ammo = TextFormat("%d/%d", weapon->ammoInMagazine, def->magazineSize);
        DrawText(ammo, (int)(slot.x + slotSize*0.5f) - MeasureText(ammo, 10)/2, (int)slot.y + 44, 10, Fade(HUD_TEXT, 0.7f));
    }
}

static void DrawEnemyHealthBars(const HudContext *context)
{
    Vector3 cameraForward = Vector3Normalize(Vector3Subtract(context->camera.target, context->camera.position));

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        const Enemy *enemy = &context->enemies->items[i];
        if (!Enemy_IsAlive(enemy)) continue;

        const EnemyDef *def = Enemy_GetDef(enemy->type);
        if (enemy->health >= def->maxHealth) continue;     // Only once damaged

        Vector3 anchor = Vector3Add(Enemy_GetHeadTop(enemy), (Vector3){ 0.0f, 0.35f, 0.0f });
        Vector3 toEnemy = Vector3Subtract(anchor, context->camera.position);
        if (Vector3DotProduct(toEnemy, cameraForward) <= 0.5f) continue;   // Behind the camera
        if (Vector3Length(toEnemy) > 50.0f) continue;
        if (!Level_HasLineOfSight(context->level, context->camera.position, anchor)) continue;

        Vector2 screen = GetWorldToScreen(anchor, context->camera);
        float fraction = Clamp(enemy->health/def->maxHealth, 0.0f, 1.0f);
        DrawRectangle((int)screen.x - 26, (int)screen.y - 4, 52, 7, Fade(BLACK, 0.7f));
        DrawRectangle((int)screen.x - 25, (int)screen.y - 3, (int)(50.0f*fraction), 5, RED);
    }
}

static void DrawHelp(const HudState *hud, const HudContext *context)
{
    if (!hud->showHelp)
    {
        DrawText("[F1] help", 10, 10, 10, Fade(HUD_TEXT, 0.6f));
        return;
    }

    const char *lines[] = {
        "WASD move, SPACE jump, CTRL crouch",
        "Mouse look, LMB fire, R reload",
        "1-7 / wheel switch weapon, Q last weapon",
        "M map, F5 new level, F1 hide help",
    };
    int lineCount = (int)(sizeof(lines)/sizeof(lines[0]));

    DrawRectangle(5, 5, 290, 20 + lineCount*15 + 18, HUD_PANEL);
    for (int i = 0; i < lineCount; i++) DrawText(lines[i], 12, 12 + i*15, 10, HUD_TEXT);
    DrawText(TextFormat("Seed %u  |  Enemies %d  |  %d FPS", context->seed, Enemies_CountAlive(context->enemies), GetFPS()),
             12, 12 + lineCount*15 + 4, 10, HUD_ACCENT);
}

static void DrawDeathScreen(void)
{
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    const char *title = "YOU DIED";
    const char *hint = "ENTER restart level   |   F5 new level";

    DrawRectangle(0, 0, screenWidth, screenHeight, Fade((Color){ 80, 0, 0, 255 }, 0.55f));
    DrawText(title, screenWidth/2 - MeasureText(title, 60)/2, screenHeight/2 - 50, 60, RAYWHITE);
    DrawText(hint, screenWidth/2 - MeasureText(hint, 20)/2, screenHeight/2 + 20, 20, RAYWHITE);
}

void Hud_Draw(const HudState *hud, HudContext context)
{
    const Player *player = context.player;

    DrawViewModel(player);
    DrawEnemyHealthBars(&context);
    if (player->isAlive) DrawCrosshair(hud, player);

    if (player->damageFlash > 0.0f) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(RED, player->damageFlash*0.35f));

    DrawHealth(player);
    DrawAmmo(player);
    DrawInventoryBar(player);
    Minimap_Draw(context.minimap, context.level, player, context.enemies);
    DrawHelp(hud, &context);

    if (!player->isAlive) DrawDeathScreen();
}
