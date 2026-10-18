#include "player/PlayerInput.h"
#include "weapons/Inventory.h"

PlayerInput PlayerInput::Read()
{
    PlayerInput input;

    input.lookDelta = GetMouseDelta();
    input.sideways = static_cast<int>(IsKeyDown(KEY_D)) - static_cast<int>(IsKeyDown(KEY_A));
    input.forward = static_cast<int>(IsKeyDown(KEY_W)) - static_cast<int>(IsKeyDown(KEY_S));
    input.jumpPressed = IsKeyPressed(KEY_SPACE);
    input.crouchHeld = IsKeyDown(KEY_LEFT_CONTROL);
    input.firePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.fireHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input.aimHeld = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    input.reloadPressed = IsKeyPressed(KEY_R);
    input.lastWeaponPressed = IsKeyPressed(KEY_Q);
    input.interactPressed = IsKeyPressed(KEY_E);

    for (int i = 0; i < Inventory::SlotCount; i++)
    {
        if (IsKeyPressed(static_cast<KeyboardKey>(KEY_ONE + i))) input.selectSlot = i;
    }

    float wheel = GetMouseWheelMove();
    input.cycleDirection = (wheel > 0.0f) ? -1 : ((wheel < 0.0f) ? 1 : 0);

    return input;
}
