#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Weapons: a static definition table + per-instance ammo/timer state
//----------------------------------------------------------------------------------
inline constexpr int InfiniteAmmo = -1;

// To add a weapon: add an entry here and fill its definition in Weapon.cpp
enum class WeaponType
{
    None = 0,
    Pistol,
    Shotgun,
    Rifle,
    Plasma,
    Chainsaw,
    Count
};

enum class FireMode
{
    Semi,       // One shot per click
    Auto        // Fires while held
};

enum class ShotKind
{
    Hitscan,    // Instant ray
    Projectile, // Spawns a travelling projectile
    Melee       // Hits everything in an arc in front of the player every tick
};

struct WeaponDef
{
    const char *name = "Empty";
    const char *shortName = "-";        // Inventory bar label
    FireMode fireMode = FireMode::Semi;
    ShotKind shotKind = ShotKind::Hitscan;
    float damage = 0.0f;                // Per pellet (melee: per tick)
    int pelletsPerShot = 1;
    float spreadDegrees = 0.0f;         // Melee: total width of the hit arc
    float range = 0.0f;                 // Hitscan range / projectile travel distance / melee reach
    float fireInterval = 0.0f;          // Seconds between shots (melee: between damage ticks)
    int magazineSize = 0;               // Shots before a reload is needed, 0 = uses no ammo
    int maxReserveAmmo = 0;             // InfiniteAmmo for unlimited
    int startingReserve = 0;            // Reserve ammo when the weapon is given
    int ammoPickupAmount = 0;           // Reserve ammo an ammo pickup adds
    float reloadTime = 0.0f;
    float switchTime = 0.0f;            // Time to lower the previous weapon and raise this one
    float projectileSpeed = 0.0f;       // ShotKind::Projectile only
    float projectileRadius = 0.0f;
    float noiseRadius = 0.0f;           // Enemies inside this radius hear the shot
    Color color = { 0, 0, 0, 0 };       // Viewmodel / tracer color
    float viewLength = 0.0f;            // Viewmodel length (fraction of screen height)
    float viewWidth = 0.0f;             // Viewmodel half width in pixels at the base
};

const WeaponDef &GetWeaponDef(WeaponType type);

class Weapon
{
public:
    Weapon() = default;
    explicit Weapon(WeaponType type);       // Full magazine and the starting reserve

    void Update(float deltaTime);           // Timers, recoil, reload completion
    bool TryFire(bool triggerPressed, bool triggerHeld);    // True when a shot happened (ammo consumed)
    bool StartReload();
    void CancelReload() { m_reloadTimer = 0.0f; }
    int AddAmmo(int amount);                // For pickups; returns ammo actually added

    WeaponType GetType() const { return m_type; }
    const WeaponDef &GetDef() const { return GetWeaponDef(m_type); }
    bool IsEmpty() const { return m_type == WeaponType::None; }
    bool IsReloading() const { return m_reloadTimer > 0.0f; }
    float GetReloadProgress() const;        // 0..1
    bool HasInfiniteAmmo() const { return GetDef().maxReserveAmmo == InfiniteAmmo; }
    bool UsesAmmo() const { return GetDef().magazineSize > 0; }
    int GetAmmoInMagazine() const { return m_ammoInMagazine; }
    int GetReserveAmmo() const { return m_reserveAmmo; }
    float GetRecoil() const { return m_recoil; }
    float GetMuzzleFlash() const { return m_muzzleFlash; }

    static constexpr float MuzzleFlashTime = 0.05f;

private:
    void FinishReload();

    WeaponType m_type = WeaponType::None;
    int m_ammoInMagazine = 0;
    int m_reserveAmmo = 0;
    float m_cooldown = 0.0f;        // Time until the next shot is allowed
    float m_reloadTimer = 0.0f;     // > 0 while reloading
    float m_recoil = 0.0f;          // 0..1 visual kick, decays over time
    float m_muzzleFlash = 0.0f;     // > 0 while the flash is visible
};
