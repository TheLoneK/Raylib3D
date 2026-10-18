#pragma once

#include "raylib.h"
#include "flecs.h"
#include "ecs/GameContext.h"
#include "weapons/Weapon.h"
#include <string>

class Level;
class MessageFeed;
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
        const MessageFeed &messages;
        const std::string &prompt;         // Interaction hint under the crosshair
        Camera3D camera;
        unsigned int seed;
        int levelNumber;
        float levelTransitionTime;          // < 0 when no transition is pending
        bool customViewModel;               // An imported weapon model was drawn instead of the 2D one
    };

    void Init(flecs::world &world);
    void Update(float deltaTime);
    void RegisterHits(const CombatStats &stats);
    void ToggleHelp() { m_showHelp = !m_showHelp; }
    void Draw(const FrameInfo &frame) const;

private:
    void DrawViewModel(const Player &player) const;
    void DrawChainsaw(const Player &player, const WeaponDef &def, Vector2 base, Vector2 tip) const;
    void DrawBow(const Player &player, const Weapon &weapon, Vector2 offset) const;
    void DrawIronSights(const Player &player, Vector2 tip) const;
    void DrawLevelTransition(float timeLeft) const;
    void DrawStatusEffects(const Player &player) const;
    void DrawMessages(const FrameInfo &frame) const;
    void DrawBossBar() const;
    void DrawCrosshair(const Player &player) const;
    void DrawHealth(const Player &player) const;
    void DrawAmmo(const Player &player) const;
    void DrawInventoryBar(const Player &player) const;
    void DrawEnemyHealthBars(const FrameInfo &frame) const;
    void DrawHelp(const FrameInfo &frame) const;
    void DrawDeathScreen() const;

    float m_hitMarkerTimer = 0.0f;
    float m_killMarkerTimer = 0.0f;
    float m_headshotTimer = 0.0f;
    float m_immuneTimer = 0.0f;
    bool m_showHelp = true;
    flecs::query<> m_livingEnemies;
    flecs::query<> m_bosses;
    flecs::query<> m_crystals;
};
