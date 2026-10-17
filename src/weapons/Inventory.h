#pragma once

#include "weapons/Weapon.h"
#include <array>
#include <cstddef>

//----------------------------------------------------------------------------------
// Weapon inventory: fixed slots selected with keys 1-7 / mouse wheel
//----------------------------------------------------------------------------------
class Inventory
{
public:
    static constexpr int SlotCount = 7;
    static constexpr int NoSlot = -1;

    int GiveWeapon(WeaponType type);                // First free slot (or adds ammo if owned). Returns slot or NoSlot
    bool SetSlot(int slot, WeaponType type);        // Place a weapon in a specific slot
    bool IsSlotEmpty(int slot) const;
    void SetActiveSlot(int slot);                   // Instant, no switch animation

    bool SelectSlot(int slot);
    void SelectNext(int direction);                 // +1 / -1, skips empty slots
    void SelectPrevious();                          // Swap to last used weapon

    void Update(float deltaTime);

    Weapon *GetActiveWeapon();                      // nullptr when the active slot is empty
    const Weapon *GetActiveWeapon() const;
    const Weapon &GetSlot(int slot) const { return m_slots[static_cast<size_t>(slot)]; }
    int GetActiveSlot() const { return m_activeSlot; }
    int GetSelectedSlot() const { return IsSwitching() ? m_pendingSlot : m_activeSlot; }
    bool IsSwitching() const { return m_pendingSlot != NoSlot; }
    float GetLowerAmount() const;                   // 0 = raised, 1 = fully lowered (viewmodel)

private:
    static bool IsValidSlot(int slot) { return (slot >= 0) && (slot < SlotCount); }

    std::array<Weapon, SlotCount> m_slots = {};     // Default Weapon = empty slot
    int m_activeSlot = 0;
    int m_previousSlot = 0;
    int m_pendingSlot = NoSlot;                     // Slot being switched to
    float m_switchTimer = 0.0f;                     // Counts down while switching
    float m_switchDuration = 0.0f;
};
