#include "weapon.h"
#include "raymath.h"

//----------------------------------------------------------------------------------
// Weapon definitions
//----------------------------------------------------------------------------------
static const WeaponDef weaponDefs[WEAPON_TYPE_COUNT] = {
    [WEAPON_NONE] = {
        .name = "Empty", .shortName = "-",
    },
    [WEAPON_PISTOL] = {
        .name = "Pistol", .shortName = "PSTL",
        .fireMode = FIRE_MODE_SEMI, .shotKind = SHOT_HITSCAN,
        .damage = 25.0f, .pelletsPerShot = 1, .spreadDegrees = 0.6f, .range = 100.0f,
        .fireInterval = 0.15f, .magazineSize = 12, .maxReserveAmmo = WEAPON_INFINITE_AMMO,
        .reloadTime = 1.1f, .switchTime = 0.35f, .noiseRadius = 25.0f,
        .color = (Color){ 150, 150, 160, 255 }, .viewLength = 0.24f, .viewWidth = 34.0f,
    },
    [WEAPON_SHOTGUN] = {
        .name = "Shotgun", .shortName = "SHTG",
        .fireMode = FIRE_MODE_SEMI, .shotKind = SHOT_HITSCAN,
        .damage = 12.0f, .pelletsPerShot = 8, .spreadDegrees = 6.0f, .range = 40.0f,
        .fireInterval = 0.8f, .magazineSize = 6, .maxReserveAmmo = 36,
        .reloadTime = 2.0f, .switchTime = 0.5f, .noiseRadius = 35.0f,
        .color = (Color){ 140, 100, 70, 255 }, .viewLength = 0.30f, .viewWidth = 48.0f,
    },
    [WEAPON_RIFLE] = {
        .name = "Rifle", .shortName = "RIFL",
        .fireMode = FIRE_MODE_AUTO, .shotKind = SHOT_HITSCAN,
        .damage = 14.0f, .pelletsPerShot = 1, .spreadDegrees = 1.5f, .range = 120.0f,
        .fireInterval = 0.09f, .magazineSize = 30, .maxReserveAmmo = 120,
        .reloadTime = 1.8f, .switchTime = 0.45f, .noiseRadius = 30.0f,
        .color = (Color){ 80, 95, 80, 255 }, .viewLength = 0.36f, .viewWidth = 40.0f,
    },
    [WEAPON_PLASMA] = {
        .name = "Plasma Gun", .shortName = "PLSM",
        .fireMode = FIRE_MODE_AUTO, .shotKind = SHOT_PROJECTILE,
        .damage = 30.0f, .pelletsPerShot = 1, .spreadDegrees = 0.5f, .range = 120.0f,
        .fireInterval = 0.22f, .magazineSize = 20, .maxReserveAmmo = 60,
        .reloadTime = 2.2f, .switchTime = 0.5f, .noiseRadius = 20.0f,
        .projectileSpeed = 45.0f, .projectileRadius = 0.18f,
        .color = (Color){ 70, 200, 230, 255 }, .viewLength = 0.28f, .viewWidth = 52.0f,
    },
};

const WeaponDef *Weapon_GetDef(WeaponType type)
{
    if ((type < 0) || (type >= WEAPON_TYPE_COUNT)) type = WEAPON_NONE;
    return &weaponDefs[type];
}

Weapon Weapon_Create(WeaponType type)
{
    const WeaponDef *def = Weapon_GetDef(type);
    return (Weapon){
        .type = type,
        .ammoInMagazine = def->magazineSize,
        .reserveAmmo = def->maxReserveAmmo,
    };
}

//----------------------------------------------------------------------------------
// State
//----------------------------------------------------------------------------------
bool Weapon_HasInfiniteAmmo(const Weapon *weapon)
{
    return (Weapon_GetDef(weapon->type)->maxReserveAmmo == WEAPON_INFINITE_AMMO);
}

bool Weapon_IsReloading(const Weapon *weapon)
{
    return (weapon->reloadTimer > 0.0f);
}

float Weapon_GetReloadProgress(const Weapon *weapon)
{
    const WeaponDef *def = Weapon_GetDef(weapon->type);
    if (!Weapon_IsReloading(weapon) || (def->reloadTime <= 0.0f)) return 0.0f;
    return 1.0f - weapon->reloadTimer/def->reloadTime;
}

static void FinishReload(Weapon *weapon)
{
    const WeaponDef *def = Weapon_GetDef(weapon->type);
    int needed = def->magazineSize - weapon->ammoInMagazine;
    int taken = Weapon_HasInfiniteAmmo(weapon) ? needed : (int)fminf((float)needed, (float)weapon->reserveAmmo);

    weapon->ammoInMagazine += taken;
    if (!Weapon_HasInfiniteAmmo(weapon)) weapon->reserveAmmo -= taken;
}

bool Weapon_StartReload(Weapon *weapon)
{
    const WeaponDef *def = Weapon_GetDef(weapon->type);

    if (weapon->type == WEAPON_NONE) return false;
    if (Weapon_IsReloading(weapon)) return false;
    if (weapon->ammoInMagazine >= def->magazineSize) return false;
    if (!Weapon_HasInfiniteAmmo(weapon) && (weapon->reserveAmmo <= 0)) return false;

    weapon->reloadTimer = def->reloadTime;
    return true;
}

void Weapon_CancelReload(Weapon *weapon)
{
    weapon->reloadTimer = 0.0f;
}

void Weapon_Update(Weapon *weapon, float deltaTime)
{
    weapon->cooldown = fmaxf(0.0f, weapon->cooldown - deltaTime);
    weapon->muzzleFlash = fmaxf(0.0f, weapon->muzzleFlash - deltaTime);
    weapon->recoil = Lerp(weapon->recoil, 0.0f, Clamp(12.0f*deltaTime, 0.0f, 1.0f));

    if (Weapon_IsReloading(weapon))
    {
        weapon->reloadTimer -= deltaTime;
        if (weapon->reloadTimer <= 0.0f)
        {
            weapon->reloadTimer = 0.0f;
            FinishReload(weapon);
        }
    }
}

bool Weapon_TryFire(Weapon *weapon, bool triggerPressed, bool triggerHeld)
{
    const WeaponDef *def = Weapon_GetDef(weapon->type);
    bool trigger = (def->fireMode == FIRE_MODE_AUTO) ? triggerHeld : triggerPressed;

    if (!trigger || (weapon->type == WEAPON_NONE)) return false;
    if (Weapon_IsReloading(weapon) || (weapon->cooldown > 0.0f)) return false;

    // Dry fire starts a reload instead
    if (weapon->ammoInMagazine <= 0)
    {
        Weapon_StartReload(weapon);
        return false;
    }

    weapon->ammoInMagazine--;
    weapon->cooldown = def->fireInterval;
    weapon->recoil = 1.0f;
    weapon->muzzleFlash = 0.05f;

    // Auto reload once the magazine runs dry
    if (weapon->ammoInMagazine == 0) Weapon_StartReload(weapon);

    return true;
}

int Weapon_AddAmmo(Weapon *weapon, int amount)
{
    const WeaponDef *def = Weapon_GetDef(weapon->type);
    if ((weapon->type == WEAPON_NONE) || Weapon_HasInfiniteAmmo(weapon) || (amount <= 0)) return 0;

    int space = def->maxReserveAmmo - weapon->reserveAmmo;
    int added = (int)fminf((float)amount, (float)space);
    weapon->reserveAmmo += added;
    return added;
}
