#pragma once

#include "raylib.h"
#include "assets/AssetManifest.h"
#include "enemies/EnemyDefs.h"
#include "weapons/Weapon.h"
#include <span>
#include <string>

//----------------------------------------------------------------------------------
// Asset slots: the things in the game that can show an imported model/texture.
// Each slot has a stable id ("enemy.brute", "weapon.shotgun") used as the key in
// the asset library file, so reordering the enums never breaks saved bindings.
//----------------------------------------------------------------------------------
enum class SlotKind
{
    Enemy,      // Drawn in the world at the enemy's position, fit to its collider height
    Weapon      // Drawn as the first person viewmodel
};

struct AssetSlot
{
    SlotKind kind;
    int type;               // EnemyType or WeaponType as an int
    const char *id;
    const char *label;      // Shown in the editor
};

// First person weapon layout in view space (eye at the origin looking down -Z, +X right, +Y up).
// Shared by the game's ViewModel3D and the editor's first person preview
namespace ViewModelLayout
{
    inline constexpr float FieldOfView = 60.0f;         // Fixed, so aiming zoom does not scale the gun
    inline constexpr float WeaponLength = 0.4f;         // Auto fit: longest side of the model
    inline constexpr Vector3 HipPosition = { 0.16f, -0.15f, -0.5f };
    inline constexpr Vector3 AimPosition = { 0.0f, -0.075f, -0.42f };
    Camera3D GetCamera();
}

std::span<const AssetSlot> GetAssetSlots();
float GetAssetSlotFitSize(const AssetSlot &slot);       // Size the game fits the model to (enemy height, weapon length)
FitMode GetAssetSlotFitMode(const AssetSlot &slot);
const AssetSlot *FindAssetSlot(const std::string &id);
const char *GetEnemySlotId(EnemyType type);
const char *GetWeaponSlotId(WeaponType type);
float GetEnemyFitSize(EnemyType type);
