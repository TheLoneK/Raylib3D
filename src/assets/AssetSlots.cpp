#include "assets/AssetSlots.h"
#include <array>

namespace
{
    constexpr int EnemyIndex(EnemyType type) { return static_cast<int>(type); }
    constexpr int WeaponIndex(WeaponType type) { return static_cast<int>(type); }

    // To make a new enemy or weapon skinnable: add a row here and check for its binding where it is drawn
    constexpr std::array<AssetSlot, 11> s_slots = {{
        { SlotKind::Enemy, EnemyIndex(EnemyType::Brute), "enemy.brute", "Brute" },
        { SlotKind::Enemy, EnemyIndex(EnemyType::Gunner), "enemy.gunner", "Gunner" },
        { SlotKind::Enemy, EnemyIndex(EnemyType::SkullLord), "enemy.skull_lord", "Skull Lord" },
        { SlotKind::Enemy, EnemyIndex(EnemyType::Lich), "enemy.lich", "Lich" },
        { SlotKind::Enemy, EnemyIndex(EnemyType::LichCrystal), "enemy.lich_crystal", "Soul Crystal" },
        { SlotKind::Weapon, WeaponIndex(WeaponType::Pistol), "weapon.pistol", "Pistol" },
        { SlotKind::Weapon, WeaponIndex(WeaponType::Shotgun), "weapon.shotgun", "Shotgun" },
        { SlotKind::Weapon, WeaponIndex(WeaponType::Rifle), "weapon.rifle", "Rifle" },
        { SlotKind::Weapon, WeaponIndex(WeaponType::Plasma), "weapon.plasma", "Plasma Gun" },
        { SlotKind::Weapon, WeaponIndex(WeaponType::Chainsaw), "weapon.chainsaw", "Chainsaw" },
        { SlotKind::Weapon, WeaponIndex(WeaponType::Bow), "weapon.bow", "Bow" },
    }};

    const char *FindId(SlotKind kind, int type)
    {
        for (const AssetSlot &slot : s_slots)
        {
            if ((slot.kind == kind) && (slot.type == type)) return slot.id;
        }
        return "";
    }
}

namespace ViewModelLayout
{
    Camera3D GetCamera()
    {
        Camera3D camera = {};
        camera.position = Vector3{ 0.0f, 0.0f, 0.0f };
        camera.target = Vector3{ 0.0f, 0.0f, -1.0f };
        camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
        camera.fovy = FieldOfView;
        camera.projection = CAMERA_PERSPECTIVE;
        return camera;
    }
}

float GetAssetSlotFitSize(const AssetSlot &slot)
{
    if (slot.kind == SlotKind::Weapon) return ViewModelLayout::WeaponLength;

    EnemyType type = static_cast<EnemyType>(slot.type);
    const EnemyDef &def = GetEnemyDef(type);
    if (type == EnemyType::SkullLord) return def.radius*2.0f;      // Fills the floating skull sphere
    if (type == EnemyType::LichCrystal) return 1.8f;               // The built-in diamond, not the hit cylinder
    return def.height;
}

FitMode GetAssetSlotFitMode(const AssetSlot &slot)
{
    return (slot.kind == SlotKind::Weapon) ? FitMode::Length : FitMode::Height;
}

float GetEnemyFitSize(EnemyType type)
{
    const AssetSlot *slot = FindAssetSlot(GetEnemySlotId(type));
    return (slot != nullptr) ? GetAssetSlotFitSize(*slot) : GetEnemyDef(type).height;
}

std::span<const AssetSlot> GetAssetSlots()
{
    return s_slots;
}

const AssetSlot *FindAssetSlot(const std::string &id)
{
    for (const AssetSlot &slot : s_slots)
    {
        if (id == slot.id) return &slot;
    }
    return nullptr;
}

const char *GetEnemySlotId(EnemyType type)
{
    return FindId(SlotKind::Enemy, EnemyIndex(type));
}

const char *GetWeaponSlotId(WeaponType type)
{
    return FindId(SlotKind::Weapon, WeaponIndex(type));
}
