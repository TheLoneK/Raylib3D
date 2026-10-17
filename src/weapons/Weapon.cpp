#include "weapons/Weapon.h"
#include "raymath.h"
#include <algorithm>
#include <array>

//----------------------------------------------------------------------------------
// Weapon definitions (fields must stay in declaration order)
//----------------------------------------------------------------------------------
static const std::array<WeaponDef, static_cast<size_t>(WeaponType::Count)> s_weaponDefs = {{
    // WeaponType::None
    WeaponDef{},
    // WeaponType::Pistol
    WeaponDef{
        .name = "Pistol", .shortName = "PSTL",
        .fireMode = FireMode::Semi, .shotKind = ShotKind::Hitscan,
        .damage = 25.0f, .pelletsPerShot = 1, .spreadDegrees = 0.6f, .range = 100.0f,
        .fireInterval = 0.15f, .magazineSize = 12, .maxReserveAmmo = 96, .startingReserve = 48, .ammoPickupAmount = 24,
        .reloadTime = 1.1f, .switchTime = 0.35f, .noiseRadius = 25.0f,
        .color = Color{ 150, 150, 160, 255 }, .viewLength = 0.24f, .viewWidth = 34.0f,
    },
    // WeaponType::Shotgun
    WeaponDef{
        .name = "Shotgun", .shortName = "SHTG",
        .fireMode = FireMode::Semi, .shotKind = ShotKind::Hitscan,
        .damage = 12.0f, .pelletsPerShot = 8, .spreadDegrees = 6.0f, .range = 40.0f,
        .fireInterval = 0.8f, .magazineSize = 6, .maxReserveAmmo = 36, .startingReserve = 12, .ammoPickupAmount = 12,
        .reloadTime = 2.0f, .switchTime = 0.5f, .noiseRadius = 35.0f,
        .color = Color{ 140, 100, 70, 255 }, .viewLength = 0.30f, .viewWidth = 48.0f,
    },
    // WeaponType::Rifle
    WeaponDef{
        .name = "Rifle", .shortName = "RIFL",
        .fireMode = FireMode::Auto, .shotKind = ShotKind::Hitscan,
        .damage = 14.0f, .pelletsPerShot = 1, .spreadDegrees = 1.5f, .range = 120.0f,
        .fireInterval = 0.09f, .magazineSize = 30, .maxReserveAmmo = 180, .startingReserve = 60, .ammoPickupAmount = 60,
        .reloadTime = 1.8f, .switchTime = 0.45f, .noiseRadius = 30.0f,
        .color = Color{ 80, 95, 80, 255 }, .viewLength = 0.36f, .viewWidth = 40.0f,
    },
    // WeaponType::Plasma
    WeaponDef{
        .name = "Plasma Gun", .shortName = "PLSM",
        .fireMode = FireMode::Auto, .shotKind = ShotKind::Projectile,
        .damage = 30.0f, .pelletsPerShot = 1, .spreadDegrees = 0.5f, .range = 120.0f,
        .fireInterval = 0.22f, .magazineSize = 20, .maxReserveAmmo = 80, .startingReserve = 20, .ammoPickupAmount = 30,
        .reloadTime = 2.2f, .switchTime = 0.5f,
        .projectileSpeed = 45.0f, .projectileRadius = 0.18f, .noiseRadius = 20.0f,
        .color = Color{ 70, 200, 230, 255 }, .viewLength = 0.28f, .viewWidth = 52.0f,
    },
    // WeaponType::Chainsaw (no ammo, constant damage in a short arc while held)
    WeaponDef{
        .name = "Chainsaw", .shortName = "SAW",
        .fireMode = FireMode::Auto, .shotKind = ShotKind::Melee,
        .damage = 9.0f, .pelletsPerShot = 1, .spreadDegrees = 70.0f, .range = 2.4f,
        .fireInterval = 0.1f, .magazineSize = 0, .maxReserveAmmo = InfiniteAmmo,
        .switchTime = 0.4f, .noiseRadius = 12.0f,
        .color = Color{ 215, 160, 40, 255 }, .viewLength = 0.32f, .viewWidth = 30.0f,
    },
}};

const WeaponDef &GetWeaponDef(WeaponType type)
{
    size_t index = static_cast<size_t>(type);
    if (index >= s_weaponDefs.size()) index = 0;
    return s_weaponDefs[index];
}

//----------------------------------------------------------------------------------
// State
//----------------------------------------------------------------------------------
Weapon::Weapon(WeaponType type)
    : m_type(type),
      m_ammoInMagazine(GetWeaponDef(type).magazineSize),
      m_reserveAmmo(GetWeaponDef(type).startingReserve)
{
}

float Weapon::GetReloadProgress() const
{
    const WeaponDef &def = GetDef();
    if (!IsReloading() || (def.reloadTime <= 0.0f)) return 0.0f;
    return 1.0f - m_reloadTimer/def.reloadTime;
}

void Weapon::FinishReload()
{
    int needed = GetDef().magazineSize - m_ammoInMagazine;
    int taken = HasInfiniteAmmo() ? needed : std::min(needed, m_reserveAmmo);

    m_ammoInMagazine += taken;
    if (!HasInfiniteAmmo()) m_reserveAmmo -= taken;
}

bool Weapon::StartReload()
{
    if (IsEmpty() || !UsesAmmo() || IsReloading()) return false;
    if (m_ammoInMagazine >= GetDef().magazineSize) return false;
    if (!HasInfiniteAmmo() && (m_reserveAmmo <= 0)) return false;

    m_reloadTimer = GetDef().reloadTime;
    return true;
}

void Weapon::Update(float deltaTime)
{
    m_cooldown = std::max(0.0f, m_cooldown - deltaTime);
    m_muzzleFlash = std::max(0.0f, m_muzzleFlash - deltaTime);
    m_recoil = Lerp(m_recoil, 0.0f, Clamp(12.0f*deltaTime, 0.0f, 1.0f));

    if (IsReloading())
    {
        m_reloadTimer -= deltaTime;
        if (m_reloadTimer <= 0.0f)
        {
            m_reloadTimer = 0.0f;
            FinishReload();
        }
    }
}

bool Weapon::TryFire(bool triggerPressed, bool triggerHeld)
{
    const WeaponDef &def = GetDef();
    bool trigger = (def.fireMode == FireMode::Auto) ? triggerHeld : triggerPressed;

    if (!trigger || IsEmpty()) return false;
    if (IsReloading() || (m_cooldown > 0.0f)) return false;

    // Weapons without ammo (melee) just tick at their fire interval
    if (!UsesAmmo())
    {
        m_cooldown = def.fireInterval;
        m_recoil = 0.3f;
        m_muzzleFlash = def.fireInterval*1.5f;     // Doubles as an "is running" flag for the viewmodel
        return true;
    }

    // Dry fire starts a reload instead
    if (m_ammoInMagazine <= 0)
    {
        StartReload();
        return false;
    }

    m_ammoInMagazine--;
    m_cooldown = def.fireInterval;
    m_recoil = 1.0f;
    m_muzzleFlash = MuzzleFlashTime;

    // Auto reload once the magazine runs dry
    if (m_ammoInMagazine == 0) StartReload();

    return true;
}

int Weapon::AddAmmo(int amount)
{
    if (IsEmpty() || !UsesAmmo() || HasInfiniteAmmo() || (amount <= 0)) return 0;

    int added = std::min(amount, GetDef().maxReserveAmmo - m_reserveAmmo);
    m_reserveAmmo += added;
    return added;
}
