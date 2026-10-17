#pragma once

#include "raylib.h"
#include "flecs.h"
#include "ecs/GameContext.h"

class Level;
class Player;
class Minimap;

//----------------------------------------------------------------------------------
// 2D overlay: viewmodel, crosshair, health, ammo, inventory bar, enemy health bars
//----------------------------------------------------------------------------------
class Hud
{
public:
    // Everything the HUD reads in one frame
    struct FrameInfo
    {
        const Player &player;
        const Level &level;
        const Minimap &minimap;
        Camera3D camera;
        unsigned int seed;
    };

    void Init(flecs::world &world);
    void Update(float deltaTime);
    void RegisterHits(const CombatStats &stats);
    void ToggleHelp() { m_showHelp = !m_showHelp; }
    void Draw(const FrameInfo &frame) const;

private:
    void DrawViewModel(const Player &player) const;
    void DrawCrosshair(const Player &player) const;
    void DrawHealth(const Player &player) const;
    void DrawAmmo(const Player &player) const;
    void DrawInventoryBar(const Player &player) const;
    void DrawEnemyHealthBars(const FrameInfo &frame) const;
    void DrawHelp(const FrameInfo &frame) const;
    void DrawDeathScreen() const;

    float m_hitMarkerTimer = 0.0f;
    float m_killMarkerTimer = 0.0f;
    bool m_showHelp = true;
    flecs::query<> m_livingEnemies;
};
