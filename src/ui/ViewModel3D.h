#pragma once

#include "raylib.h"

class AssetLibrary;
class Player;

//----------------------------------------------------------------------------------
// First person weapon drawn from an imported model (bound with the asset editor).
// Rendered into its own render texture so the world never clips into the gun, then
// laid over the screen. Weapons without a model keep the Hud's 2D viewmodel.
// Holds a GPU render texture: destroy it before the window closes.
//----------------------------------------------------------------------------------
class ViewModel3D
{
public:
    ViewModel3D() = default;
    ~ViewModel3D();
    ViewModel3D(const ViewModel3D &) = delete;
    ViewModel3D &operator=(const ViewModel3D &) = delete;

    // Draws the active weapon's model; false when it has none (draw the 2D viewmodel instead)
    bool Draw(const Player &player, AssetLibrary &library);

private:
    RenderTexture2D m_target = {};
};
