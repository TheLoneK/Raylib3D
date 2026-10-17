#include "weapons/Inventory.h"
#include <algorithm>
#include <cmath>

bool Inventory::IsSlotEmpty(int slot) const
{
    return !IsValidSlot(slot) || m_slots[static_cast<size_t>(slot)].IsEmpty();
}

bool Inventory::SetSlot(int slot, WeaponType type)
{
    if (!IsValidSlot(slot)) return false;
    m_slots[static_cast<size_t>(slot)] = Weapon(type);
    return true;
}

void Inventory::SetActiveSlot(int slot)
{
    if (!IsValidSlot(slot)) return;
    m_activeSlot = slot;
    m_previousSlot = slot;
    m_pendingSlot = NoSlot;
}

int Inventory::GiveWeapon(WeaponType type)
{
    // Already owned: top up the reserve instead
    for (int i = 0; i < SlotCount; i++)
    {
        Weapon &weapon = m_slots[static_cast<size_t>(i)];
        if (weapon.GetType() == type)
        {
            weapon.AddAmmo(weapon.GetDef().magazineSize);
            return i;
        }
    }

    for (int i = 0; i < SlotCount; i++)
    {
        if (IsSlotEmpty(i))
        {
            SetSlot(i, type);
            return i;
        }
    }

    return NoSlot;
}

bool Inventory::SelectSlot(int slot)
{
    if (IsSlotEmpty(slot) || (slot == GetSelectedSlot())) return false;

    m_slots[static_cast<size_t>(m_activeSlot)].CancelReload();

    m_previousSlot = m_activeSlot;
    m_pendingSlot = slot;
    m_switchDuration = std::max(m_slots[static_cast<size_t>(slot)].GetDef().switchTime, 0.01f);
    m_switchTimer = m_switchDuration;
    return true;
}

void Inventory::SelectNext(int direction)
{
    if (direction == 0) return;

    int start = GetSelectedSlot();
    int step = (direction > 0) ? 1 : -1;

    for (int i = 1; i < SlotCount; i++)
    {
        int slot = (start + step*i + SlotCount)%SlotCount;
        if (!IsSlotEmpty(slot))
        {
            SelectSlot(slot);
            return;
        }
    }
}

void Inventory::SelectPrevious()
{
    SelectSlot(m_previousSlot);
}

void Inventory::Update(float deltaTime)
{
    for (Weapon &weapon : m_slots) weapon.Update(deltaTime);

    if (!IsSwitching()) return;

    m_switchTimer -= deltaTime;

    // The new weapon becomes active halfway through (old one fully lowered)
    if (m_switchTimer <= m_switchDuration*0.5f) m_activeSlot = m_pendingSlot;

    if (m_switchTimer <= 0.0f)
    {
        m_switchTimer = 0.0f;
        m_activeSlot = m_pendingSlot;
        m_pendingSlot = NoSlot;
    }
}

Weapon *Inventory::FindWeapon(WeaponType type)
{
    for (Weapon &weapon : m_slots)
    {
        if (!weapon.IsEmpty() && (weapon.GetType() == type)) return &weapon;
    }
    return nullptr;
}

Weapon *Inventory::GetActiveWeapon()
{
    if (IsSlotEmpty(m_activeSlot)) return nullptr;
    return &m_slots[static_cast<size_t>(m_activeSlot)];
}

const Weapon *Inventory::GetActiveWeapon() const
{
    if (IsSlotEmpty(m_activeSlot)) return nullptr;
    return &m_slots[static_cast<size_t>(m_activeSlot)];
}

float Inventory::GetLowerAmount() const
{
    if (!IsSwitching()) return 0.0f;
    float t = m_switchTimer/m_switchDuration;      // 1 -> 0
    return 1.0f - std::fabs(2.0f*t - 1.0f);        // 0 -> 1 -> 0
}
