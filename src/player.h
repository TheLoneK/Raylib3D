#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "inventory.h"
#include "level.h"

//----------------------------------------------------------------------------------
// Player: movement, look, health and the weapon inventory
//----------------------------------------------------------------------------------
#define PLAYER_RADIUS           0.4f
#define PLAYER_MAX_HEALTH       100.0f

// One frame of input, read once so the rest of the game never touches the keyboard directly
typedef struct PlayerInput {
    Vector2 lookDelta;
    signed char sideways;       // -1 left, +1 right
    signed char forward;        // -1 back, +1 forward
    bool jumpPressed;
    bool crouchHeld;
    bool firePressed;
    bool fireHeld;
    bool reloadPressed;
    bool lastWeaponPressed;
    int selectSlot;             // INVENTORY_NO_SLOT when no number key was pressed
    int cycleDirection;         // Mouse wheel: -1, 0, +1
} PlayerInput;

typedef struct Player {
    Vector3 position;           // Feet
    Vector3 velocity;
    Vector3 moveDirection;
    bool isGrounded;
    float yaw;                  // Radians, 0 looks down -Z
    float pitch;                // Radians, positive looks up
    Vector2 lean;               // Camera roll/pitch offsets from strafing
    float headHeight;           // Smoothed crouch height

    float health;
    float maxHealth;
    bool isAlive;
    float damageFlash;          // 0..1 HUD red flash

    Inventory inventory;
} Player;

PlayerInput Player_ReadInput(void);

void Player_Init(Player *player, Vector3 spawnPosition, float yaw);
void Player_Update(Player *player, const PlayerInput *input, const Level *level, float deltaTime);
void Player_UpdateCamera(const Player *player, Camera3D *camera);
void Player_TakeDamage(Player *player, float amount);
void Player_Heal(Player *player, float amount);

Vector3 Player_GetEyePosition(const Player *player);
Vector3 Player_GetCenter(const Player *player);         // Chest height, used as an aim point
Vector3 Player_GetForward(const Player *player);        // Full 3D look direction
BoundingBox Player_GetBounds(const Player *player);
float Player_GetHorizontalSpeed(const Player *player);

#endif // PLAYER_H
