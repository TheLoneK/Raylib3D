#include "player/Player.h"
#include "core/Config.h"
#include "core/MathUtils.h"
#include "raymath.h"
#include <algorithm>

//----------------------------------------------------------------------------------
// Movement tuning (based on the raylib "3d camera fps" example)
//----------------------------------------------------------------------------------
namespace
{
    constexpr float Gravity = 32.0f;
    constexpr float MaxSpeed = 20.0f;
    constexpr float CrouchSpeed = 5.0f;
    constexpr float JumpForce = 12.0f;
    constexpr float MaxAccel = 150.0f;
    constexpr float Friction = 0.86f;       // Grounded drag
    constexpr float AirDrag = 0.98f;        // Increasing air drag increases strafing speed
    constexpr float Control = 15.0f;        // Responsiveness for turning movement toward look direction
    constexpr float CrouchHeight = 0.0f;
    constexpr float StandHeight = 1.0f;
    constexpr float BottomHeight = 0.5f;
    constexpr float HeadClearance = 0.2f;

    constexpr float MouseSensitivity = 0.001f;
    constexpr float AimSpeed = 6.0f;            // Aim transitions per second
    constexpr float AimMoveScale = 0.5f;        // Movement speed while fully aimed
    constexpr float AimSpreadScale = 0.3f;      // Weapon spread while fully aimed
    constexpr float PitchLimit = PI*0.5f - 0.01f;

    Vector3 DirectionFromAngles(float yaw, float pitch)
    {
        Vector3 flat = MathUtils::DirectionFromYaw(yaw);
        return Vector3{ flat.x*cosf(pitch), sinf(pitch), flat.z*cosf(pitch) };
    }
}

//----------------------------------------------------------------------------------
// Lifecycle
//----------------------------------------------------------------------------------
void Player::Spawn(Vector3 position, float yaw)
{
    *this = Player();       // Reset everything, including the inventory
    m_position = position;
    m_yaw = yaw;
}

void Player::Update(const PlayerInput &input, const Level &level, float deltaTime)
{
    m_damageFlash = std::max(0.0f, m_damageFlash - deltaTime*2.0f);
    if (!m_isAlive) return;

    m_damageBoostTimer = std::max(0.0f, m_damageBoostTimer - deltaTime);

    // Look (slower while zoomed so the crosshair moves the same amount on screen)
    float sensitivity = MouseSensitivity*(GetFov()/Config::CameraFov);
    m_yaw -= input.lookDelta.x*sensitivity;
    m_pitch = Clamp(m_pitch - input.lookDelta.y*sensitivity, -PitchLimit, PitchLimit);

    UpdateMovement(input, level, deltaTime);

    float crouchBlend = Clamp(20.0f*deltaTime, 0.0f, 1.0f);
    float leanBlend = Clamp(10.0f*deltaTime, 0.0f, 1.0f);
    m_headHeight = Lerp(m_headHeight, input.crouchHeld ? CrouchHeight : StandHeight, crouchBlend);
    m_lean.x = Lerp(m_lean.x, static_cast<float>(input.sideways)*0.02f, leanBlend);
    m_lean.y = Lerp(m_lean.y, static_cast<float>(input.forward)*0.015f, leanBlend);

    UpdateInventoryInput(input);
    m_inventory.Update(deltaTime);
    UpdateAim(input, deltaTime);
}

void Player::UpdateAim(const PlayerInput &input, float deltaTime)
{
    const Weapon *weapon = m_inventory.GetActiveWeapon();
    bool canAim = (weapon != nullptr) && (weapon->GetDef().aimFov > 0.0f) && !weapon->IsReloading() && !m_inventory.IsSwitching();
    float target = (input.aimHeld && canAim) ? 1.0f : 0.0f;
    float step = AimSpeed*deltaTime;
    m_aimAmount = (m_aimAmount < target) ? std::min(target, m_aimAmount + step) : std::max(target, m_aimAmount - step);
}

float Player::GetFov() const
{
    const Weapon *weapon = m_inventory.GetActiveWeapon();
    if ((weapon == nullptr) || (weapon->GetDef().aimFov <= 0.0f)) return Config::CameraFov;

    float eased = m_aimAmount*m_aimAmount*(3.0f - 2.0f*m_aimAmount);    // Smoothstep
    return Lerp(Config::CameraFov, weapon->GetDef().aimFov, eased);
}

float Player::GetSpreadScale() const
{
    return Lerp(1.0f, AimSpreadScale, m_aimAmount);
}

void Player::CarryOverFrom(const Player &previous)
{
    m_inventory = previous.m_inventory;
    m_inventory.SetActiveSlot(previous.m_inventory.GetSelectedSlot());
    m_health = std::max(previous.m_health, MaxHealth*0.5f);     // Start the next level with at least half health
    m_godMode = previous.m_godMode;
}

//----------------------------------------------------------------------------------
// Movement + wall collision
//----------------------------------------------------------------------------------
void Player::UpdateMovement(const PlayerInput &input, const Level &level, float deltaTime)
{
    Vector2 move = Vector2{ static_cast<float>(input.sideways), static_cast<float>(-input.forward) };
    if ((input.sideways != 0) && (input.forward != 0)) move = Vector2Normalize(move);    // Slow down diagonal movement

    if (!m_isGrounded) m_velocity.y -= Gravity*deltaTime;

    if (m_isGrounded && input.jumpPressed)
    {
        m_velocity.y = JumpForce;
        m_isGrounded = false;
    }

    Vector3 front = Vector3{ sinf(m_yaw), 0.0f, cosf(m_yaw) };
    Vector3 right = Vector3{ cosf(-m_yaw), 0.0f, sinf(-m_yaw) };

    Vector3 desiredDir = right*move.x + front*move.y;
    m_moveDirection = Vector3Lerp(m_moveDirection, desiredDir, Clamp(Control*deltaTime, 0.0f, 1.0f));

    float decel = m_isGrounded ? Friction : AirDrag;
    Vector3 hvel = Vector3{ m_velocity.x*decel, 0.0f, m_velocity.z*decel };
    if (Vector3Length(hvel) < (MaxSpeed*0.01f)) hvel = Vector3{ 0.0f, 0.0f, 0.0f };

    // Clamping the added speed (not the total) is what allows air strafing
    float speed = Vector3DotProduct(hvel, m_moveDirection);
    float maxSpeed = (input.crouchHeld ? CrouchSpeed : MaxSpeed)*Lerp(1.0f, AimMoveScale, m_aimAmount);
    float accel = Clamp(maxSpeed - speed, 0.0f, MaxAccel*deltaTime);
    hvel += m_moveDirection*accel;

    m_velocity.x = hvel.x;
    m_velocity.z = hvel.z;
    m_position += m_velocity*deltaTime;

    // Walls: push out and remove the velocity going into the wall so we slide along it
    Vector3 normal = { 0.0f, 0.0f, 0.0f };
    if (level.ResolveCircle(m_position, Radius, &normal))
    {
        float into = Vector3DotProduct(m_velocity, normal);
        if (into < 0.0f) m_velocity -= normal*into;
    }

    // Floor
    if (m_position.y <= 0.0f)
    {
        m_position.y = 0.0f;
        m_velocity.y = 0.0f;
        m_isGrounded = true;
    }
}

void Player::UpdateInventoryInput(const PlayerInput &input)
{
    if (input.selectSlot != Inventory::NoSlot) m_inventory.SelectSlot(input.selectSlot);
    if (input.cycleDirection != 0) m_inventory.SelectNext(input.cycleDirection);
    if (input.lastWeaponPressed) m_inventory.SelectPrevious();

    Weapon *weapon = m_inventory.GetActiveWeapon();
    if (input.reloadPressed && (weapon != nullptr) && !m_inventory.IsSwitching()) weapon->StartReload();
}

//----------------------------------------------------------------------------------
// Camera
//----------------------------------------------------------------------------------
void Player::UpdateCamera(Camera3D &camera) const
{
    Vector3 eye = GetEyePosition();
    float viewPitch = Clamp(m_pitch - m_lean.y, -PitchLimit, PitchLimit);
    Vector3 forward = DirectionFromAngles(m_yaw, viewPitch);

    camera.position = eye;
    camera.target = eye + forward;
    camera.fovy = GetFov();
    camera.up = Vector3RotateByAxisAngle(Vector3{ 0.0f, 1.0f, 0.0f }, forward, m_lean.x);
}

//----------------------------------------------------------------------------------
// Health
//----------------------------------------------------------------------------------
void Player::TakeDamage(float amount)
{
    if (!m_isAlive || (amount <= 0.0f)) return;

    m_damageFlash = std::min(1.0f, m_damageFlash + 0.4f + amount/40.0f);
    if (m_godMode) return;

    m_health -= amount;
    if (m_health <= 0.0f)
    {
        m_health = 0.0f;
        m_isAlive = false;
    }
}

void Player::TakeDamageOverTime(float amount)
{
    if (!m_isAlive || (amount <= 0.0f)) return;

    m_damageFlash = std::max(m_damageFlash, 0.35f);
    if (m_godMode) return;

    m_health -= amount;
    if (m_health <= 0.0f)
    {
        m_health = 0.0f;
        m_isAlive = false;
    }
}

bool Player::Heal(float amount)
{
    if (!m_isAlive || (m_health >= MaxHealth)) return false;
    m_health = std::min(MaxHealth, m_health + amount);
    return true;
}

//----------------------------------------------------------------------------------
// Queries
//----------------------------------------------------------------------------------
Vector3 Player::GetEyePosition() const
{
    float eyeHeight = m_isAlive ? (BottomHeight + m_headHeight) : 0.3f;
    return m_position + Vector3{ 0.0f, eyeHeight, 0.0f };
}

Vector3 Player::GetCenter() const
{
    return m_position + Vector3{ 0.0f, (BottomHeight + m_headHeight)*0.65f, 0.0f };
}

Vector3 Player::GetForward() const
{
    return DirectionFromAngles(m_yaw, m_pitch);
}

BoundingBox Player::GetBounds() const
{
    float top = BottomHeight + m_headHeight + HeadClearance;
    return BoundingBox{
        Vector3{ m_position.x - Radius, m_position.y, m_position.z - Radius },
        Vector3{ m_position.x + Radius, m_position.y + top, m_position.z + Radius },
    };
}

float Player::GetHorizontalSpeed() const
{
    return Vector2Length(Vector2{ m_velocity.x, m_velocity.z });
}
