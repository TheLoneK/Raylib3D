#include "player.h"
#include "utils.h"
#include "raymath.h"
#include <stddef.h>

//----------------------------------------------------------------------------------
// Movement tuning
//----------------------------------------------------------------------------------
#define GRAVITY             32.0f
#define MAX_SPEED           20.0f
#define CROUCH_SPEED         5.0f
#define JUMP_FORCE          12.0f
#define MAX_ACCEL          150.0f
#define FRICTION             0.86f      // Grounded drag
#define AIR_DRAG             0.98f      // Increasing air drag increases strafing speed
#define CONTROL             15.0f       // Responsiveness for turning movement toward look direction
#define CROUCH_HEIGHT        0.0f
#define STAND_HEIGHT         1.0f
#define BOTTOM_HEIGHT        0.5f
#define HEAD_CLEARANCE       0.2f

#define MOUSE_SENSITIVITY    0.001f
#define PITCH_LIMIT         (PI*0.5f - 0.01f)

//----------------------------------------------------------------------------------
// Input
//----------------------------------------------------------------------------------
PlayerInput Player_ReadInput(void)
{
    PlayerInput input = { 0 };

    input.lookDelta = GetMouseDelta();
    input.sideways = (signed char)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
    input.forward = (signed char)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
    input.jumpPressed = IsKeyPressed(KEY_SPACE);
    input.crouchHeld = IsKeyDown(KEY_LEFT_CONTROL);
    input.firePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.fireHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input.reloadPressed = IsKeyPressed(KEY_R);
    input.lastWeaponPressed = IsKeyPressed(KEY_Q);

    input.selectSlot = INVENTORY_NO_SLOT;
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++)
    {
        if (IsKeyPressed((KeyboardKey)(KEY_ONE + i))) input.selectSlot = i;
    }

    float wheel = GetMouseWheelMove();
    input.cycleDirection = (wheel > 0.0f) ? -1 : ((wheel < 0.0f) ? 1 : 0);

    return input;
}

//----------------------------------------------------------------------------------
// Lifecycle
//----------------------------------------------------------------------------------
void Player_Init(Player *player, Vector3 spawnPosition, float yaw)
{
    *player = (Player){ 0 };
    player->position = spawnPosition;
    player->yaw = yaw;
    player->headHeight = STAND_HEIGHT;
    player->isGrounded = true;
    player->maxHealth = PLAYER_MAX_HEALTH;
    player->health = PLAYER_MAX_HEALTH;
    player->isAlive = true;

    Inventory_Init(&player->inventory);
}

//----------------------------------------------------------------------------------
// Movement (based on the raylib "3d camera fps" example) + wall collision
//----------------------------------------------------------------------------------
static void UpdateMovement(Player *player, const PlayerInput *input, const Level *level, float deltaTime)
{
    Vector2 move = (Vector2){ (float)input->sideways, (float)-input->forward };
    if ((input->sideways != 0) && (input->forward != 0)) move = Vector2Normalize(move);    // Slow down diagonal movement

    if (!player->isGrounded) player->velocity.y -= GRAVITY*deltaTime;

    if (player->isGrounded && input->jumpPressed)
    {
        player->velocity.y = JUMP_FORCE;
        player->isGrounded = false;
    }

    Vector3 front = (Vector3){ sinf(player->yaw), 0.0f, cosf(player->yaw) };
    Vector3 right = (Vector3){ cosf(-player->yaw), 0.0f, sinf(-player->yaw) };

    Vector3 desiredDir = (Vector3){ move.x*right.x + move.y*front.x, 0.0f, move.x*right.z + move.y*front.z };
    player->moveDirection = Vector3Lerp(player->moveDirection, desiredDir, Clamp(CONTROL*deltaTime, 0.0f, 1.0f));

    float decel = player->isGrounded ? FRICTION : AIR_DRAG;
    Vector3 hvel = (Vector3){ player->velocity.x*decel, 0.0f, player->velocity.z*decel };
    if (Vector3Length(hvel) < (MAX_SPEED*0.01f)) hvel = (Vector3){ 0 };

    // Clamping the added speed (not the total) is what allows air strafing
    float speed = Vector3DotProduct(hvel, player->moveDirection);
    float maxSpeed = input->crouchHeld ? CROUCH_SPEED : MAX_SPEED;
    float accel = Clamp(maxSpeed - speed, 0.0f, MAX_ACCEL*deltaTime);
    hvel.x += player->moveDirection.x*accel;
    hvel.z += player->moveDirection.z*accel;

    player->velocity.x = hvel.x;
    player->velocity.z = hvel.z;

    player->position = Vector3Add(player->position, Vector3Scale(player->velocity, deltaTime));

    // Walls: push out and remove the velocity going into the wall so we slide along it
    Vector3 normal = { 0 };
    if (Level_ResolveCircle(level, &player->position, PLAYER_RADIUS, &normal))
    {
        float into = Vector3DotProduct(player->velocity, normal);
        if (into < 0.0f) player->velocity = Vector3Subtract(player->velocity, Vector3Scale(normal, into));
    }

    // Floor
    if (player->position.y <= 0.0f)
    {
        player->position.y = 0.0f;
        player->velocity.y = 0.0f;
        player->isGrounded = true;
    }
}

static void UpdateInventoryInput(Player *player, const PlayerInput *input)
{
    Inventory *inventory = &player->inventory;

    if (input->selectSlot != INVENTORY_NO_SLOT) Inventory_SelectSlot(inventory, input->selectSlot);
    if (input->cycleDirection != 0) Inventory_SelectNext(inventory, input->cycleDirection);
    if (input->lastWeaponPressed) Inventory_SelectPrevious(inventory);

    Weapon *weapon = Inventory_GetActiveWeapon(inventory);
    if (input->reloadPressed && (weapon != NULL) && !Inventory_IsSwitching(inventory)) Weapon_StartReload(weapon);
}

void Player_Update(Player *player, const PlayerInput *input, const Level *level, float deltaTime)
{
    player->damageFlash = fmaxf(0.0f, player->damageFlash - deltaTime*2.0f);
    if (!player->isAlive) return;

    // Look
    player->yaw -= input->lookDelta.x*MOUSE_SENSITIVITY;
    player->pitch -= input->lookDelta.y*MOUSE_SENSITIVITY;
    player->pitch = Clamp(player->pitch, -PITCH_LIMIT, PITCH_LIMIT);

    UpdateMovement(player, input, level, deltaTime);

    player->headHeight = Lerp(player->headHeight, input->crouchHeld ? CROUCH_HEIGHT : STAND_HEIGHT, Clamp(20.0f*deltaTime, 0.0f, 1.0f));
    player->lean.x = Lerp(player->lean.x, (float)input->sideways*0.02f, Clamp(10.0f*deltaTime, 0.0f, 1.0f));
    player->lean.y = Lerp(player->lean.y, (float)input->forward*0.015f, Clamp(10.0f*deltaTime, 0.0f, 1.0f));

    UpdateInventoryInput(player, input);
    Inventory_Update(&player->inventory, deltaTime);
}

//----------------------------------------------------------------------------------
// Camera
//----------------------------------------------------------------------------------
static Vector3 DirectionFromAngles(float yaw, float pitch)
{
    Vector3 flat = Util_DirectionFromYaw(yaw);
    return (Vector3){ flat.x*cosf(pitch), sinf(pitch), flat.z*cosf(pitch) };
}

void Player_UpdateCamera(const Player *player, Camera3D *camera)
{
    Vector3 eye = Player_GetEyePosition(player);
    float viewPitch = Clamp(player->pitch - player->lean.y, -PITCH_LIMIT, PITCH_LIMIT);
    Vector3 forward = DirectionFromAngles(player->yaw, viewPitch);

    camera->position = eye;
    camera->target = Vector3Add(eye, forward);
    camera->up = Vector3RotateByAxisAngle((Vector3){ 0.0f, 1.0f, 0.0f }, forward, player->lean.x);
}

//----------------------------------------------------------------------------------
// Health
//----------------------------------------------------------------------------------
void Player_TakeDamage(Player *player, float amount)
{
    if (!player->isAlive || (amount <= 0.0f)) return;

    player->health -= amount;
    player->damageFlash = fminf(1.0f, player->damageFlash + 0.4f + amount/40.0f);

    if (player->health <= 0.0f)
    {
        player->health = 0.0f;
        player->isAlive = false;
    }
}

void Player_Heal(Player *player, float amount)
{
    if (!player->isAlive) return;
    player->health = fminf(player->maxHealth, player->health + amount);
}

//----------------------------------------------------------------------------------
// Queries
//----------------------------------------------------------------------------------
Vector3 Player_GetEyePosition(const Player *player)
{
    float eyeHeight = player->isAlive ? (BOTTOM_HEIGHT + player->headHeight) : 0.3f;
    return (Vector3){ player->position.x, player->position.y + eyeHeight, player->position.z };
}

Vector3 Player_GetCenter(const Player *player)
{
    float centerHeight = (BOTTOM_HEIGHT + player->headHeight)*0.65f;
    return (Vector3){ player->position.x, player->position.y + centerHeight, player->position.z };
}

Vector3 Player_GetForward(const Player *player)
{
    return DirectionFromAngles(player->yaw, player->pitch);
}

BoundingBox Player_GetBounds(const Player *player)
{
    float top = BOTTOM_HEIGHT + player->headHeight + HEAD_CLEARANCE;
    return (BoundingBox){
        .min = (Vector3){ player->position.x - PLAYER_RADIUS, player->position.y, player->position.z - PLAYER_RADIUS },
        .max = (Vector3){ player->position.x + PLAYER_RADIUS, player->position.y + top, player->position.z + PLAYER_RADIUS },
    };
}

float Player_GetHorizontalSpeed(const Player *player)
{
    return Vector2Length((Vector2){ player->velocity.x, player->velocity.z });
}
