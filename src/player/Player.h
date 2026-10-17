#pragma once

#include "raylib.h"
#include "player/PlayerInput.h"
#include "weapons/Inventory.h"
#include "world/Level.h"

//----------------------------------------------------------------------------------
// Player: movement, look, health and the weapon inventory
//----------------------------------------------------------------------------------
class Player
{
public:
    static constexpr float Radius = 0.4f;
    static constexpr float MaxHealth = 100.0f;

    void Spawn(Vector3 position, float yaw);
    void Update(const PlayerInput &input, const Level &level, float deltaTime);
    void UpdateCamera(Camera3D &camera) const;

    void TakeDamage(float amount);
    void Heal(float amount);
    void SetGodMode(bool enabled) { m_godMode = enabled; }

    Vector3 GetPosition() const { return m_position; }
    Vector3 GetEyePosition() const;
    Vector3 GetCenter() const;                  // Chest height, used as an aim point
    Vector3 GetForward() const;                 // Full 3D look direction
    BoundingBox GetBounds() const;
    float GetYaw() const { return m_yaw; }
    float GetHorizontalSpeed() const;
    bool IsGrounded() const { return m_isGrounded; }
    bool IsAlive() const { return m_isAlive; }
    bool IsGodMode() const { return m_godMode; }
    float GetHealth() const { return m_health; }
    float GetDamageFlash() const { return m_damageFlash; }

    Inventory &GetInventory() { return m_inventory; }
    const Inventory &GetInventory() const { return m_inventory; }

    // Debug/test helpers
    void SetPosition(Vector3 position) { m_position = position; }
    void SetLook(float yaw, float pitch) { m_yaw = yaw; m_pitch = pitch; }

private:
    void UpdateMovement(const PlayerInput &input, const Level &level, float deltaTime);
    void UpdateInventoryInput(const PlayerInput &input);

    Vector3 m_position = { 0.0f, 0.0f, 0.0f };     // Feet
    Vector3 m_velocity = { 0.0f, 0.0f, 0.0f };
    Vector3 m_moveDirection = { 0.0f, 0.0f, 0.0f };
    bool m_isGrounded = true;
    float m_yaw = 0.0f;                 // Radians, 0 looks down -Z
    float m_pitch = 0.0f;               // Radians, positive looks up
    Vector2 m_lean = { 0.0f, 0.0f };    // Camera roll/pitch offsets from strafing
    float m_headHeight = 1.0f;          // Smoothed crouch height

    float m_health = MaxHealth;
    bool m_isAlive = true;
    bool m_godMode = false;
    float m_damageFlash = 0.0f;         // 0..1 HUD red flash

    Inventory m_inventory;
};
