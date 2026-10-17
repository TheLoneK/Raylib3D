#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// One frame of player input, read once so gameplay code never polls the keyboard
//----------------------------------------------------------------------------------
struct PlayerInput
{
    Vector2 lookDelta = { 0.0f, 0.0f };
    int sideways = 0;           // -1 left, +1 right
    int forward = 0;            // -1 back, +1 forward
    bool jumpPressed = false;
    bool crouchHeld = false;
    bool firePressed = false;
    bool fireHeld = false;
    bool reloadPressed = false;
    bool lastWeaponPressed = false;
    int selectSlot = -1;        // Inventory slot from the number keys, -1 when none
    int cycleDirection = 0;     // Mouse wheel: -1, 0, +1

    static PlayerInput Read();
};
