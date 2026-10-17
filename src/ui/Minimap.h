#pragma once

#include "raylib.h"
#include "flecs.h"

class Level;
class Player;

//----------------------------------------------------------------------------------
// Minimap: compact player-centered view in the corner, or an expanded full map
//----------------------------------------------------------------------------------
struct MinimapSettings
{
    float compactSize = 200.0f;         // Pixels (square)
    float compactWorldRadius = 40.0f;   // World units visible from the center to the edge
    float margin = 16.0f;               // Pixels from the screen edge
    bool showEnemies = true;            // Later this can depend on line of sight / radar items
    Color backgroundColor = { 10, 12, 18, 200 };
    Color floorColor = { 70, 78, 90, 230 };
    Color wallColor = { 150, 140, 165, 255 };
    Color borderColor = { 200, 200, 210, 255 };
    Color playerColor = { 80, 230, 120, 255 };
};

class Minimap
{
public:
    void Init(flecs::world &world, const MinimapSettings &settings = MinimapSettings());
    void ToggleExpanded() { m_expanded = !m_expanded; }
    void Draw(const Level &level, const Player &player) const;

private:
    // How world space maps onto the minimap rectangle for one frame
    struct MapView
    {
        Rectangle bounds;
        Vector2 center;     // Screen position of focus
        Vector3 focus;      // World position at the center
        float scale;        // Pixels per world unit

        Vector2 WorldToMap(Vector3 world) const;    // North-up: world X -> screen X, world Z -> screen Y
    };

    MapView GetMapView(const Level &level, const Player &player) const;
    void DrawTiles(const MapView &view, const Level &level) const;
    void DrawEnemies(const MapView &view) const;
    void DrawPlayerArrow(const MapView &view, const Player &player) const;

    MinimapSettings m_settings;
    bool m_expanded = false;
    flecs::query<> m_livingEnemies;
};
