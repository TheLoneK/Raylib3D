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
    static constexpr float BoostedDamageMultiplier = 2.0f;

    void Spawn(Vector3 position, float yaw);
    void Update(const PlayerInput &input, const Level &level, float deltaTime);
    void UpdateCamera(Camera3D &camera) const;

    void TakeDamage(float amount);
    void TakeDamageOverTime(float amount);      // Continuous sources (hazards): no flash spam
    bool Heal(float amount);                    // False when already at full health
    void SetGodMode(bool enabled) { m_godMode = enabled; }

    // Power-ups and keys
    void AddDamageBoost(float seconds) { m_damageBoostTimer += seconds; }
    float GetDamageBoostTime() const { return m_damageBoostTimer; }
    // Aim down sights
    float GetAimAmount() const { return m_aimAmount; }  // 0 = hip, 1 = fully aimed
    float GetFov() const;                               // Camera field of view including aim zoom
    float GetSpreadScale() const;                       // Aiming tightens spread

    float GetDamageMultiplier() const { return (m_damageBoostTimer > 0.0f) ? BoostedDamageMultiplier : 1.0f; }
    void AddKey(int link) { m_keys |= (1u << static_cast<unsigned int>(link)); }
    bool HasKey(int link) const { return (m_keys & (1u << static_cast<unsigned int>(link))) != 0u; }
    unsigned int GetKeys() const { return m_keys; }    // Bit per link tag

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

    // Keeps weapons/ammo/health when moving to the next level (keys and boosts are per level)
    void CarryOverFrom(const Player &previous);

    // Debug/test helpers
    void SetPosition(Vector3 position) { m_position = position; }
    void SetLook(float yaw, float pitch) { m_yaw = yaw; m_pitch = pitch; }

private:
    void UpdateMovement(const PlayerInput &input, const Level &level, float deltaTime);
    void UpdateInventoryInput(const PlayerInput &input);
    void UpdateAim(const PlayerInput &input, float deltaTime);

    Vector3 m_position = { 0.0f, 0.0f, 0.0f };     // Feet
    Vector3 m_velocity = { 0.0f, 0.0f, 0.0f };
    Vector3 m_moveDirection = { 0.0f, 0.0f, 0.0f };
    bool m_isGrounded = true;
    float m_yaw = 0.0f;                 // Radians, 0 looks down -Z
    float m_pitch = 0.0f;               // Radians, positive looks up
    Vector2 m_lean = { 0.0f, 0.0f };    // Camera roll/pitch offsets from strafing
    float m_headHeight = 1.0f;          // Smoothed crouch height
    float m_aimAmount = 0.0f;

    float m_health = MaxHealth;
    bool m_isAlive = true;
    bool m_godMode = false;
    float m_damageFlash = 0.0f;         // 0..1 HUD red flash
    float m_damageBoostTimer = 0.0f;
    unsigned int m_keys = 0;            // Bit per link tag

    Inventory m_inventory;
};
