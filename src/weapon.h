#ifndef WEAPON_H
#define WEAPON_H

#include "raylib.h"

//----------------------------------------------------------------------------------
// Weapons: a static definition table + per-instance ammo/timer state
//----------------------------------------------------------------------------------
#define WEAPON_INFINITE_AMMO    (-1)

// To add a weapon: add an entry here and fill its definition in weapon.c
typedef enum WeaponType {
    WEAPON_NONE = 0,
    WEAPON_PISTOL,
    WEAPON_SHOTGUN,
    WEAPON_RIFLE,
    WEAPON_PLASMA,
    WEAPON_TYPE_COUNT
} WeaponType;

typedef enum FireMode {
    FIRE_MODE_SEMI = 0,     // One shot per click
    FIRE_MODE_AUTO          // Fires while held
} FireMode;

typedef enum ShotKind {
    SHOT_HITSCAN = 0,       // Instant ray
    SHOT_PROJECTILE         // Spawns a travelling projectile
} ShotKind;

typedef struct WeaponDef {
    const char *name;
    const char *shortName;      // Inventory bar label
    FireMode fireMode;
    ShotKind shotKind;
    float damage;               // Per pellet
    int pelletsPerShot;
    float spreadDegrees;
    float range;                // Hitscan range / projectile lifetime distance
    float fireInterval;         // Seconds between shots
    int magazineSize;           // Shots before a reload is needed
    int maxReserveAmmo;         // WEAPON_INFINITE_AMMO for unlimited
    float reloadTime;
    float switchTime;           // Time to lower the previous weapon and raise this one
    float projectileSpeed;      // SHOT_PROJECTILE only
    float projectileRadius;
    float noiseRadius;          // Enemies inside this radius hear the shot
    Color color;                // Viewmodel / tracer color
    float viewLength;           // Viewmodel barrel length (fraction of screen height)
    float viewWidth;            // Viewmodel half width in pixels at the base
} WeaponDef;

typedef struct Weapon {
    WeaponType type;
    int ammoInMagazine;
    int reserveAmmo;
    float cooldown;             // Time until the next shot is allowed
    float reloadTimer;          // > 0 while reloading
    float recoil;               // 0..1 visual kick, decays over time
    float muzzleFlash;          // > 0 while the flash is visible
} Weapon;

const WeaponDef *Weapon_GetDef(WeaponType type);
Weapon Weapon_Create(WeaponType type);                      // Full magazine and full reserve

void Weapon_Update(Weapon *weapon, float deltaTime);        // Timers, recoil, reload completion
bool Weapon_TryFire(Weapon *weapon, bool triggerPressed, bool triggerHeld); // True when a shot happened (ammo consumed)
bool Weapon_StartReload(Weapon *weapon);
void Weapon_CancelReload(Weapon *weapon);
bool Weapon_IsReloading(const Weapon *weapon);
float Weapon_GetReloadProgress(const Weapon *weapon);      // 0..1
bool Weapon_HasInfiniteAmmo(const Weapon *weapon);
int Weapon_AddAmmo(Weapon *weapon, int amount);             // For pickups; returns ammo actually added

#endif // WEAPON_H
