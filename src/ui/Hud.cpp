#include "ui/Hud.h"
#include "core/Render.h"
#include "ecs/Components.h"
#include "enemies/Enemies.h"
#include "player/Player.h"
#include "core/MessageFeed.h"
#include "ui/Minimap.h"
#include "world/LinkTags.h"
#include "world/Level.h"
#include "raymath.h"
#include <algorithm>
#include <iterator>
#include <string>

namespace
{
    constexpr Color HudText = { 235, 235, 240, 255 };
    constexpr Color HudPanel = { 0, 0, 0, 140 };
    constexpr Color HudAccent = { 255, 200, 80, 255 };

    void DrawQuad2D(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color)
    {
        Render::DrawTriangle2D(a, b, c, color);
        Render::DrawTriangle2D(a, c, d, color);
    }

    void DrawTextCentered(const char *text, int centerX, int y, int fontSize, Color color)
    {
        DrawText(text, centerX - MeasureText(text, fontSize)/2, y, fontSize, color);
    }
}

void Hud::Init(flecs::world &world)
{
    m_livingEnemies = world.query_builder()
        .with<Enemy>()
        .with<WorldTransform>()
        .with<Collider>()
        .with<Health>()
        .without<Dead>()
        .without<Boss>()
        .build();

    m_bosses = world.query_builder()
        .with<Boss>()
        .with<Health>()
        .with<Enemy>()
        .without<Dead>()
        .build();

    m_crystals = world.query_builder()
        .with<LichCrystal>()
        .without<Dead>()
        .build();
}

void Hud::Update(float deltaTime)
{
    m_hitMarkerTimer = std::max(0.0f, m_hitMarkerTimer - deltaTime);
    m_killMarkerTimer = std::max(0.0f, m_killMarkerTimer - deltaTime);
    m_headshotTimer = std::max(0.0f, m_headshotTimer - deltaTime);
    m_immuneTimer = std::max(0.0f, m_immuneTimer - deltaTime);
}

void Hud::RegisterHits(const CombatStats &stats)
{
    if (stats.hits > 0) m_hitMarkerTimer = 0.15f;
    if (stats.kills > 0) m_killMarkerTimer = 0.4f;
    if (stats.headshots > 0) m_headshotTimer = 0.35f;
    if (stats.immuneHits > 0) m_immuneTimer = 0.3f;
}

//----------------------------------------------------------------------------------
// Viewmodel (procedural 2D weapon drawn over the 3D view)
//----------------------------------------------------------------------------------
void Hud::DrawViewModel(const Player &player) const
{
    const Inventory &inventory = player.GetInventory();
    const Weapon *weapon = inventory.GetActiveWeapon();
    if ((weapon == nullptr) || !player.IsAlive()) return;

    const WeaponDef &def = weapon->GetDef();
    float screenWidth = static_cast<float>(GetScreenWidth());
    float screenHeight = static_cast<float>(GetScreenHeight());

    // Offsets: switching lowers the gun, reloading dips it, walking bobs it, firing kicks it
    float lower = inventory.GetLowerAmount();
    float reloadDip = sinf(weapon->GetReloadProgress()*PI);
    float walk = player.IsGrounded() ? Clamp(player.GetHorizontalSpeed()/20.0f, 0.0f, 1.0f) : 0.0f;
    float time = static_cast<float>(GetTime());

    // Aiming steadies the bob and pulls the gun to the middle of the screen
    float aim = player.GetAimAmount();
    float aimEased = aim*aim*(3.0f - 2.0f*aim);
    float sway = 1.0f - aimEased*0.85f;
    Vector2 offset = Vector2{
        (sinf(time*8.0f)*10.0f*walk)*sway + weapon->GetRecoil()*6.0f,
        lower*screenHeight*0.45f + reloadDip*screenHeight*0.22f + (fabsf(cosf(time*8.0f))*8.0f*walk)*sway + weapon->GetRecoil()*18.0f,
    };

    if (def.fireMode == FireMode::Charge)
    {
        DrawBow(player, *weapon, offset);
        return;
    }

    Vector2 center = Vector2{ screenWidth*0.5f, screenHeight*0.5f };
    Vector2 hipBase = Vector2{ screenWidth*0.70f, screenHeight + 20.0f };
    Vector2 hipTip = Vector2{ screenWidth*0.60f, screenHeight*(1.0f - def.viewLength) };
    Vector2 aimBase = Vector2{ center.x, screenHeight + 80.0f };
    Vector2 aimTip = Vector2{ center.x, center.y + 16.0f };     // Front sight post reaches the screen center
    Vector2 base = Vector2Lerp(hipBase, aimBase, aimEased) + offset;
    Vector2 tip = Vector2Lerp(hipTip, aimTip, aimEased) + offset;

    if (def.shotKind == ShotKind::Melee)
    {
        DrawChainsaw(player, def, base, tip);
        return;
    }

    Vector2 direction = Vector2Normalize(tip - base);
    Vector2 side = Vector2{ -direction.y, direction.x };
    float baseHalf = def.viewWidth;
    float tipHalf = def.viewWidth*0.45f;

    Vector2 baseRight = base + side*baseHalf;
    Vector2 baseLeft = base - side*baseHalf;
    Vector2 tipRight = tip + side*tipHalf;
    Vector2 tipLeft = tip - side*tipHalf;
    Color outline = Fade(BLACK, 0.6f);

    // Body, lit left face, outline
    DrawQuad2D(baseRight, baseLeft, tipLeft, tipRight, Render::Shade(def.color, 0.5f));
    DrawQuad2D(baseLeft, base - side*(baseHalf*0.35f), tip - side*(tipHalf*0.35f), tipLeft, def.color);
    DrawLineEx(baseRight, tipRight, 2.0f, outline);
    DrawLineEx(baseLeft, tipLeft, 2.0f, outline);
    DrawLineEx(tipLeft, tipRight, 2.0f, outline);

    // Muzzle opening
    DrawCircleV(tip, tipHalf*0.55f, Render::Shade(def.color, 0.25f));
    DrawCircleV(tip, tipHalf*0.3f, Color{ 10, 10, 12, 255 });

    // Hand gripping the gun
    Vector2 hand = base + direction*90.0f + side*(baseHalf*0.3f);
    DrawCircleV(hand - direction*60.0f, baseHalf*0.85f, Color{ 60, 70, 60, 255 });     // Sleeve
    DrawCircleV(hand, baseHalf*0.6f, Color{ 205, 160, 130, 255 });

    if (aimEased > 0.3f) DrawIronSights(player, tip);

    if (weapon->GetMuzzleFlash() > 0.0f)
    {
        float flash = weapon->GetMuzzleFlash()/Weapon::MuzzleFlashTime;
        DrawCircleV(tip, 30.0f*flash + 12.0f, Fade(Color{ 255, 200, 80, 255 }, 0.8f));
        DrawCircleV(tip, 12.0f*flash + 4.0f, Color{ 255, 250, 220, 255 });
    }
}

void Hud::DrawIronSights(const Player &player, Vector2 tip) const
{
    // Front post on the muzzle, rear notch closer to the eye; lined up they frame the screen center
    float alpha = Clamp((player.GetAimAmount() - 0.3f)/0.5f, 0.0f, 1.0f);
    Color sight = Fade(Color{ 20, 20, 24, 255 }, alpha);
    Color dot = Fade(Color{ 255, 120, 60, 255 }, alpha);

    DrawRectangleV(Vector2{ tip.x - 3.0f, tip.y - 16.0f }, Vector2{ 6.0f, 16.0f }, sight);
    DrawCircleV(Vector2{ tip.x, tip.y - 14.0f }, 2.0f, dot);

    float rearY = tip.y + 60.0f;
    DrawRectangleV(Vector2{ tip.x - 40.0f, rearY - 14.0f }, Vector2{ 28.0f, 30.0f }, sight);
    DrawRectangleV(Vector2{ tip.x + 12.0f, rearY - 14.0f }, Vector2{ 28.0f, 30.0f }, sight);
    DrawRectangleV(Vector2{ tip.x - 40.0f, rearY + 10.0f }, Vector2{ 80.0f, 10.0f }, sight);
}

void Hud::DrawBow(const Player &player, const Weapon &weapon, Vector2 offset) const
{
    float screenWidth = static_cast<float>(GetScreenWidth());
    float screenHeight = static_cast<float>(GetScreenHeight());
    float aim = player.GetAimAmount();
    float draw = weapon.GetDrawAmount();
    float time = static_cast<float>(GetTime());
    const WeaponDef &def = weapon.GetDef();
    Color wood = def.color;
    Color woodDark = Render::Shade(wood, 0.6f);

    // Held upright left of center; aiming brings the arrow onto the crosshair
    Vector2 grip = Vector2Lerp(Vector2{ screenWidth*0.5f - 150.0f, screenHeight*0.5f + 120.0f }, Vector2{ screenWidth*0.5f - 40.0f, screenHeight*0.5f + 20.0f }, aim) + offset;
    float shake = (draw >= 1.0f) ? sinf(time*60.0f)*1.5f : 0.0f;    // Strain at full draw
    grip.x += shake;

    Vector2 top = grip + Vector2{ 50.0f, -270.0f };
    Vector2 bottom = grip + Vector2{ 50.0f, 270.0f };
    DrawSplineSegmentBezierQuadratic(grip, grip + Vector2{ -70.0f, -150.0f }, top, 12.0f, wood);
    DrawSplineSegmentBezierQuadratic(grip, grip + Vector2{ -70.0f, 150.0f }, bottom, 12.0f, wood);
    DrawRectangleV(grip - Vector2{ 9.0f, 30.0f }, Vector2{ 18.0f, 60.0f }, woodDark);     // Grip wrap

    // String pulled back toward the eye with the draw
    Vector2 rest = grip + Vector2{ 60.0f, 0.0f };
    Vector2 pulled = Vector2Lerp(grip + Vector2{ 240.0f, 160.0f }, grip + Vector2{ 200.0f, 230.0f }, aim);
    Vector2 nock = Vector2Lerp(rest, pulled, draw);
    DrawLineEx(top, nock, 2.0f, Color{ 230, 230, 220, 255 });
    DrawLineEx(bottom, nock, 2.0f, Color{ 230, 230, 220, 255 });

    // Arrow (hidden while a new one is being nocked)
    bool arrowReady = !weapon.IsReloading() || (weapon.GetReloadProgress() > 0.6f);
    if (arrowReady && (weapon.GetAmmoInMagazine() > 0))
    {
        Vector2 arrowTip = grip + Vector2{ -14.0f, -6.0f };
        Vector2 direction = Vector2Normalize(arrowTip - nock);
        Vector2 side = Vector2{ -direction.y, direction.x };
        DrawLineEx(nock, arrowTip, 5.0f, Color{ 150, 110, 70, 255 });
        Color head = (draw >= 1.0f) ? WHITE : Color{ 170, 170, 180, 255 };
        Render::DrawTriangle2D(arrowTip + direction*16.0f, arrowTip + side*7.0f, arrowTip - side*7.0f, head);
        Render::DrawTriangle2D(nock + direction*26.0f + side*9.0f, nock + direction*2.0f, nock + direction*26.0f - side*9.0f, Color{ 220, 60, 50, 255 });
        if (draw >= 1.0f) DrawCircleV(arrowTip + direction*10.0f, 10.0f + 3.0f*sinf(time*12.0f), Fade(WHITE, 0.35f));
    }

    // Hands: one on the grip, one on the string
    DrawCircleV(grip + Vector2{ 6.0f, 18.0f }, 22.0f, Color{ 205, 160, 130, 255 });
    DrawCircleV(nock + Vector2{ 14.0f, 10.0f }, 24.0f, Color{ 205, 160, 130, 255 });
}

void Hud::DrawChainsaw(const Player &player, const WeaponDef &def, Vector2 base, Vector2 tip) const
{
    const Weapon &weapon = *player.GetInventory().GetActiveWeapon();
    bool running = weapon.GetMuzzleFlash() > 0.0f;
    float time = static_cast<float>(GetTime());

    // Shake while cutting
    if (running)
    {
        Vector2 shake = Vector2{ sinf(time*90.0f)*3.0f, cosf(time*77.0f)*3.0f };
        base += shake;
        tip += shake;
    }

    Vector2 direction = Vector2Normalize(tip - base);
    Vector2 side = Vector2{ -direction.y, direction.x };
    float barHalf = def.viewWidth*0.55f;
    Vector2 barStart = base + direction*140.0f;

    // Guide bar with teeth running along both edges
    DrawQuad2D(barStart + side*barHalf, barStart - side*barHalf, tip - side*barHalf, tip + side*barHalf, Color{ 170, 170, 178, 255 });
    DrawCircleV(tip, barHalf, Color{ 170, 170, 178, 255 });
    float barLength = Vector2Distance(barStart, tip);
    float scroll = running ? fmodf(time*400.0f, 16.0f) : 0.0f;
    for (float t = scroll; t < barLength; t += 16.0f)
    {
        for (float edge : { -1.0f, 1.0f })
        {
            Vector2 root = barStart + direction*t + side*(barHalf*edge);
            Render::DrawTriangle2D(root, root + direction*10.0f, root + direction*4.0f + side*(7.0f*edge), Color{ 60, 60, 66, 255 });
        }
    }

    // Engine body + handle
    Vector2 bodyCenter = base + direction*70.0f;
    float bodyHalf = def.viewWidth*1.6f;
    DrawQuad2D(bodyCenter + side*bodyHalf - direction*80.0f, bodyCenter - side*bodyHalf - direction*80.0f,
               bodyCenter - side*bodyHalf + direction*80.0f, bodyCenter + side*bodyHalf + direction*80.0f, def.color);
    DrawLineEx(bodyCenter - side*bodyHalf + direction*80.0f, bodyCenter + side*bodyHalf + direction*80.0f, 3.0f, Fade(BLACK, 0.5f));
    DrawCircleV(bodyCenter + side*(bodyHalf*0.4f), def.viewWidth*0.9f, Color{ 205, 160, 130, 255 });   // Hand
}

//----------------------------------------------------------------------------------
// Crosshair / markers
//----------------------------------------------------------------------------------
void Hud::DrawCrosshair(const Player &player) const
{
    Vector2 center = Vector2{ static_cast<float>(GetScreenWidth())*0.5f, static_cast<float>(GetScreenHeight())*0.5f };
    const Weapon *weapon = player.GetInventory().GetActiveWeapon();
    float spread = (weapon != nullptr) ? weapon->GetDef().spreadDegrees : 0.0f;
    float recoil = (weapon != nullptr) ? weapon->GetRecoil() : 0.0f;
    float gap = 5.0f + spread*2.0f*player.GetSpreadScale() + recoil*8.0f;
    float length = 7.0f;

    // The sights take over while aiming
    Color lines = Fade(HudText, 1.0f - Clamp(player.GetAimAmount()*1.5f, 0.0f, 1.0f));
    DrawLineEx(Vector2{ center.x - gap - length, center.y }, Vector2{ center.x - gap, center.y }, 2.0f, lines);
    DrawLineEx(Vector2{ center.x + gap, center.y }, Vector2{ center.x + gap + length, center.y }, 2.0f, lines);
    DrawLineEx(Vector2{ center.x, center.y - gap - length }, Vector2{ center.x, center.y - gap }, 2.0f, lines);
    DrawLineEx(Vector2{ center.x, center.y + gap }, Vector2{ center.x, center.y + gap + length }, 2.0f, lines);
    DrawCircleV(center, 1.5f, lines);

    // Bow draw: red until it can fire, yellow while charging, white at full power
    if ((weapon != nullptr) && weapon->IsCharging())
    {
        const WeaponDef &def = weapon->GetDef();
        float drawn = weapon->GetDrawAmount();
        bool canFire = drawn*def.fullChargeTime >= def.minChargeTime;
        Color ring = (drawn >= 1.0f) ? WHITE : (canFire ? HudAccent : RED);
        DrawRing(center, 16.0f, 19.0f, -90.0f, -90.0f + 360.0f*drawn, 32, ring);
        float minAngle = -90.0f + 360.0f*(def.minChargeTime/def.fullChargeTime);
        DrawRing(center, 14.0f, 21.0f, minAngle - 1.5f, minAngle + 1.5f, 2, Fade(WHITE, 0.6f));
    }

    if ((m_hitMarkerTimer > 0.0f) || (m_killMarkerTimer > 0.0f))
    {
        Color color = (m_killMarkerTimer > 0.0f) ? RED : ((m_headshotTimer > 0.0f) ? HudAccent : WHITE);
        float size = (m_headshotTimer > 0.0f) ? 17.0f : 13.0f;
        for (Vector2 corner : { Vector2{ -1.0f, -1.0f }, Vector2{ 1.0f, -1.0f }, Vector2{ -1.0f, 1.0f }, Vector2{ 1.0f, 1.0f } })
        {
            DrawLineEx(center + corner*6.0f, center + corner*size, (m_headshotTimer > 0.0f) ? 3.0f : 2.0f, color);
        }
    }
    if (m_headshotTimer > 0.0f) DrawTextCentered("HEADSHOT", static_cast<int>(center.x), static_cast<int>(center.y) - 40, 10, HudAccent);
    if (m_immuneTimer > 0.0f) DrawTextCentered("IMMUNE", static_cast<int>(center.x), static_cast<int>(center.y) + 24, 10, Color{ 90, 230, 255, 255 });
}

//----------------------------------------------------------------------------------
// Status panels
//----------------------------------------------------------------------------------
void Hud::DrawHealth(const Player &player) const
{
    Rectangle panel = Rectangle{ 20.0f, static_cast<float>(GetScreenHeight()) - 70.0f, 260.0f, 50.0f };
    float fraction = Clamp(player.GetHealth()/Player::MaxHealth, 0.0f, 1.0f);
    Color barColor = (fraction > 0.5f) ? Color{ 80, 200, 110, 255 } : ((fraction > 0.25f) ? HudAccent : RED);
    int x = static_cast<int>(panel.x);
    int y = static_cast<int>(panel.y);

    DrawRectangleRec(panel, HudPanel);
    DrawText(player.IsGodMode() ? "HEALTH (GOD MODE)" : "HEALTH", x + 10, y + 6, 10, player.IsGodMode() ? HudAccent : HudText);
    DrawRectangle(x + 10, y + 22, 180, 18, Fade(BLACK, 0.6f));
    DrawRectangle(x + 10, y + 22, static_cast<int>(180.0f*fraction), 18, barColor);
    DrawText(TextFormat("%3d", static_cast<int>(ceilf(player.GetHealth()))), x + 200, y + 18, 24, HudText);
}

void Hud::DrawAmmo(const Player &player) const
{
    const Weapon *weapon = player.GetInventory().GetActiveWeapon();
    if (weapon == nullptr) return;

    const WeaponDef &def = weapon->GetDef();
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    int x = screenWidth - 220;
    int y = screenHeight - 70;

    if (!weapon->UsesAmmo())
    {
        DrawRectangle(x, y, 200, 50, HudPanel);
        DrawText(def.name, x + 10, y + 6, 10, HudText);
        DrawText("--", x + 10, y + 18, 28, HudText);
        return;
    }

    const char *reserve = weapon->HasInfiniteAmmo() ? "INF" : TextFormat("%d", weapon->GetReserveAmmo());
    bool lowAmmo = (weapon->GetAmmoInMagazine() <= def.magazineSize/4);

    DrawRectangle(x, y, 200, 50, HudPanel);
    DrawText(def.name, x + 10, y + 6, 10, HudText);
    DrawText(TextFormat("%d", weapon->GetAmmoInMagazine()), x + 10, y + 18, 28, lowAmmo ? RED : HudText);
    DrawText(TextFormat("/ %s", reserve), x + 70, y + 26, 20, Fade(HudText, 0.7f));

    int centerX = screenWidth/2;
    int centerY = screenHeight/2;
    if (weapon->IsReloading())
    {
        DrawTextCentered("RELOADING", centerX, centerY + 30, 10, HudAccent);
        DrawRectangle(centerX - 40, centerY + 44, 80, 4, Fade(BLACK, 0.6f));
        DrawRectangle(centerX - 40, centerY + 44, static_cast<int>(80.0f*weapon->GetReloadProgress()), 4, HudAccent);
    }
    else if (weapon->GetAmmoInMagazine() == 0)
    {
        bool canReload = weapon->HasInfiniteAmmo() || (weapon->GetReserveAmmo() > 0);
        DrawTextCentered(canReload ? "RELOAD [R]" : "NO AMMO", centerX, centerY + 30, 10, RED);
    }
}

void Hud::DrawInventoryBar(const Player &player) const
{
    const Inventory &inventory = player.GetInventory();
    const float slotSize = 62.0f;
    const float gap = 6.0f;
    float totalWidth = static_cast<float>(Inventory::SlotCount)*slotSize + static_cast<float>(Inventory::SlotCount - 1)*gap;
    float startX = (static_cast<float>(GetScreenWidth()) - totalWidth)*0.5f;
    float y = static_cast<float>(GetScreenHeight()) - slotSize - 14.0f;

    for (int i = 0; i < Inventory::SlotCount; i++)
    {
        Rectangle slot = Rectangle{ startX + static_cast<float>(i)*(slotSize + gap), y, slotSize, slotSize };
        const Weapon &weapon = inventory.GetSlot(i);
        bool isSelected = (i == inventory.GetSelectedSlot());
        int x = static_cast<int>(slot.x);
        int top = static_cast<int>(slot.y);
        int centerX = static_cast<int>(slot.x + slotSize*0.5f);

        DrawRectangleRec(slot, isSelected ? Fade(HudAccent, 0.35f) : HudPanel);
        DrawRectangleLinesEx(slot, isSelected ? 2.0f : 1.0f, isSelected ? HudAccent : Fade(HudText, 0.3f));
        DrawText(TextFormat("%d", i + 1), x + 4, top + 3, 10, Fade(HudText, 0.8f));

        if (weapon.IsEmpty()) continue;

        const WeaponDef &def = weapon.GetDef();
        DrawRectangle(x + 8, top + 18, static_cast<int>(slotSize) - 16, 6, def.color);
        DrawTextCentered(def.shortName, centerX, top + 30, 10, HudText);
        const char *ammo = weapon.UsesAmmo() ? TextFormat("%d/%d", weapon.GetAmmoInMagazine(), weapon.GetReserveAmmo()) : "--";
        DrawTextCentered(ammo, centerX, top + 44, 10, Fade(HudText, 0.7f));
    }
}

void Hud::DrawEnemyHealthBars(const FrameInfo &frame) const
{
    Vector3 cameraForward = Vector3Normalize(frame.camera.target - frame.camera.position);

    m_livingEnemies.each([&](flecs::entity entity) {
        const Health &health = entity.get<Health>();
        if (health.current >= health.max) return;      // Only once damaged

        const WorldTransform &transform = entity.get<WorldTransform>();
        const Collider &collider = entity.get<Collider>();
        Vector3 anchor = transform.position + Vector3{ 0.0f, collider.height + 0.35f, 0.0f };
        Vector3 toEnemy = anchor - frame.camera.position;
        if (Vector3DotProduct(toEnemy, cameraForward) <= 0.5f) return;     // Behind the camera
        if (Vector3Length(toEnemy) > 50.0f) return;
        if (!frame.level.HasLineOfSight(frame.camera.position, anchor)) return;

        Vector2 screen = GetWorldToScreen(anchor, frame.camera);
        float fraction = Clamp(health.current/health.max, 0.0f, 1.0f);
        DrawRectangle(static_cast<int>(screen.x) - 26, static_cast<int>(screen.y) - 4, 52, 7, Fade(BLACK, 0.7f));
        DrawRectangle(static_cast<int>(screen.x) - 25, static_cast<int>(screen.y) - 3, static_cast<int>(50.0f*fraction), 5, RED);
    });
}

void Hud::DrawHelp(const FrameInfo &frame) const
{
    if (!m_showHelp)
    {
        DrawText("[F1] help", 10, 10, 10, Fade(HudText, 0.6f));
        return;
    }

    const char *lines[] = {
        "WASD move, SPACE jump, CTRL crouch",
        "Mouse look, LMB fire, R reload",
        "1-7 / wheel switch weapon, Q last weapon",
        "RMB aim down sights, E use switch, M map",
        "F2 god mode, F3 spawn wave, F4 all enemies hunt",
        "F6 spawn a random boss, F7 spawn an elevator",
        "F5 new level, F1 hide help",
    };
    int lineCount = static_cast<int>(std::size(lines));

    DrawRectangle(5, 5, 320, 20 + lineCount*15 + 18, HudPanel);
    for (int i = 0; i < lineCount; i++) DrawText(lines[i], 12, 12 + i*15, 10, HudText);
    DrawText(TextFormat("Level %d  |  Seed %u  |  Enemies %d  |  %d FPS", frame.levelNumber, frame.seed, m_livingEnemies.count(), GetFPS()), 12, 12 + lineCount*15 + 4, 10, HudAccent);
}

void Hud::DrawStatusEffects(const Player &player) const
{
    int x = 20;
    int y = GetScreenHeight() - 100;

    // Damage boost countdown
    float boost = player.GetDamageBoostTime();
    if (boost > 0.0f)
    {
        Color boostColor = { 190, 70, 255, 255 };
        bool blink = (boost < 3.0f) && (fmodf(static_cast<float>(GetTime()), 0.4f) < 0.2f);
        DrawRectangle(x, y, 140, 22, HudPanel);
        DrawText(TextFormat("2X DAMAGE %2.0fs", ceilf(boost)), x + 8, y + 6, 10, blink ? WHITE : boostColor);
        x += 150;
    }

    // Keys held
    for (int link = 0; link < LinkTags::Count; link++)
    {
        if (!player.HasKey(link)) continue;
        Color color = LinkTags::GetColor(link);
        DrawRectangle(x, y, 22, 22, HudPanel);
        DrawCircle(x + 8, y + 11, 5.0f, color);
        DrawRectangle(x + 11, y + 9, 9, 4, color);
        x += 26;
    }
}

void Hud::DrawMessages(const FrameInfo &frame) const
{
    int centerX = GetScreenWidth()/2;
    int y = GetScreenHeight() - 112;

    const auto &messages = frame.messages.GetMessages();
    for (auto it = messages.rbegin(); it != messages.rend(); ++it)
    {
        float alpha = std::min(1.0f, it->timeLeft/0.5f);
        DrawTextCentered(it->text.c_str(), centerX, y, 20, Fade(it->color, alpha));
        y -= 24;
    }

    if (!frame.prompt.empty())
    {
        DrawTextCentered(frame.prompt.c_str(), centerX, GetScreenHeight()/2 + 60, 20, HudAccent);
    }
}

void Hud::DrawBossBar() const
{
    int y = 24;

    // One bar per engaged boss, stacked
    m_bosses.each([&](flecs::entity entity) {
        if (!entity.get<Boss>().engaged) return;

        const Health &health = entity.get<Health>();
        bool immune = entity.has<Invulnerable>();
        std::string name = GetEnemyDef(entity.get<Enemy>().type).name;
        if (immune)
        {
            int crystals = 0;
            m_crystals.each([&](flecs::entity crystal) { if (crystal.get<LichCrystal>().owner == entity.id()) crystals++; });
            name += TextFormat("  -  IMMUNE: destroy %d crystal%s", crystals, (crystals == 1) ? "" : "s");
        }

        int width = 500;
        int x = (GetScreenWidth() - width)/2;
        float fraction = Clamp(health.current/health.max, 0.0f, 1.0f);
        Color fill = immune ? Color{ 90, 230, 255, 255 } : Color{ 170, 70, 255, 255 };

        DrawTextCentered(name.c_str(), GetScreenWidth()/2, y - 18, 20, Color{ 230, 220, 200, 255 });
        DrawRectangle(x - 2, y - 2, width + 4, 18, Fade(BLACK, 0.7f));
        DrawRectangle(x, y, static_cast<int>(static_cast<float>(width)*fraction), 14, fill);
        DrawRectangleLines(x - 2, y - 2, width + 4, 18, Color{ 230, 220, 200, 255 });
        y += 44;
    });
}

void Hud::DrawLevelTransition(float timeLeft) const
{
    int centerX = GetScreenWidth()/2;
    int y = GetScreenHeight()/4;
    const char *title = "BOSS DEFEATED";
    const char *countdown = TextFormat("Moving to the next level in %d", static_cast<int>(ceilf(timeLeft)));

    DrawRectangle(centerX - 260, y - 14, 520, 80, Fade(BLACK, 0.55f));
    DrawTextCentered(title, centerX, y, 30, Color{ 255, 220, 120, 255 });
    DrawTextCentered(countdown, centerX, y + 38, 20, RAYWHITE);
}

void Hud::DrawDeathScreen() const
{
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(Color{ 80, 0, 0, 255 }, 0.55f));
    DrawTextCentered("YOU DIED", screenWidth/2, screenHeight/2 - 50, 60, RAYWHITE);
    DrawTextCentered("ENTER restart level   |   F5 new level", screenWidth/2, screenHeight/2 + 20, 20, RAYWHITE);
}

void Hud::Draw(const FrameInfo &frame) const
{
    const Player &player = frame.player;

    DrawViewModel(player);
    DrawEnemyHealthBars(frame);
    if (player.IsAlive()) DrawCrosshair(player);

    if (player.GetDamageFlash() > 0.0f) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(RED, player.GetDamageFlash()*0.35f));

    DrawHealth(player);
    DrawStatusEffects(player);
    DrawAmmo(player);
    DrawInventoryBar(player);
    DrawMessages(frame);
    DrawBossBar();
    if (frame.levelTransitionTime >= 0.0f) DrawLevelTransition(frame.levelTransitionTime);
    frame.minimap.Draw(frame.level, player);
    DrawHelp(frame);

    if (!player.IsAlive()) DrawDeathScreen();
}
