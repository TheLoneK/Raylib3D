#include "inventory.h"
#include "raymath.h"
#include <stddef.h>

void Inventory_Init(Inventory *inventory)
{
    *inventory = (Inventory){ 0 };
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++) inventory->slots[i] = Weapon_Create(WEAPON_NONE);
    inventory->activeSlot = 0;
    inventory->previousSlot = 0;
    inventory->pendingSlot = INVENTORY_NO_SLOT;
}

static bool IsValidSlot(int slot)
{
    return (slot >= 0) && (slot < INVENTORY_SLOT_COUNT);
}

bool Inventory_IsSlotEmpty(const Inventory *inventory, int slot)
{
    return !IsValidSlot(slot) || (inventory->slots[slot].type == WEAPON_NONE);
}

bool Inventory_SetSlot(Inventory *inventory, int slot, WeaponType type)
{
    if (!IsValidSlot(slot)) return false;
    inventory->slots[slot] = Weapon_Create(type);
    return true;
}

int Inventory_GiveWeapon(Inventory *inventory, WeaponType type)
{
    // Already owned: top up the reserve instead
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++)
    {
        if (inventory->slots[i].type == type)
        {
            Weapon_AddAmmo(&inventory->slots[i], Weapon_GetDef(type)->magazineSize);
            return i;
        }
    }

    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++)
    {
        if (Inventory_IsSlotEmpty(inventory, i))
        {
            Inventory_SetSlot(inventory, i, type);
            return i;
        }
    }

    return INVENTORY_NO_SLOT;
}

bool Inventory_SelectSlot(Inventory *inventory, int slot)
{
    if (Inventory_IsSlotEmpty(inventory, slot)) return false;

    int target = Inventory_IsSwitching(inventory) ? inventory->pendingSlot : inventory->activeSlot;
    if (slot == target) return false;

    Weapon_CancelReload(&inventory->slots[inventory->activeSlot]);

    inventory->previousSlot = inventory->activeSlot;
    inventory->pendingSlot = slot;
    inventory->switchDuration = fmaxf(Weapon_GetDef(inventory->slots[slot].type)->switchTime, 0.01f);
    inventory->switchTimer = inventory->switchDuration;
    return true;
}

void Inventory_SelectNext(Inventory *inventory, int direction)
{
    if (direction == 0) return;

    int start = Inventory_IsSwitching(inventory) ? inventory->pendingSlot : inventory->activeSlot;
    int step = (direction > 0) ? 1 : -1;

    for (int i = 1; i < INVENTORY_SLOT_COUNT; i++)
    {
        int slot = (start + step*i + INVENTORY_SLOT_COUNT)%INVENTORY_SLOT_COUNT;
        if (!Inventory_IsSlotEmpty(inventory, slot))
        {
            Inventory_SelectSlot(inventory, slot);
            return;
        }
    }
}

void Inventory_SelectPrevious(Inventory *inventory)
{
    Inventory_SelectSlot(inventory, inventory->previousSlot);
}

void Inventory_Update(Inventory *inventory, float deltaTime)
{
    for (int i = 0; i < INVENTORY_SLOT_COUNT; i++) Weapon_Update(&inventory->slots[i], deltaTime);

    if (!Inventory_IsSwitching(inventory)) return;

    inventory->switchTimer -= deltaTime;

    // The new weapon becomes active halfway through (old one fully lowered)
    if ((inventory->switchTimer <= inventory->switchDuration*0.5f) && (inventory->activeSlot != inventory->pendingSlot))
    {
        inventory->activeSlot = inventory->pendingSlot;
    }

    if (inventory->switchTimer <= 0.0f)
    {
        inventory->switchTimer = 0.0f;
        inventory->activeSlot = inventory->pendingSlot;
        inventory->pendingSlot = INVENTORY_NO_SLOT;
    }
}

Weapon *Inventory_GetActiveWeapon(Inventory *inventory)
{
    if (Inventory_IsSlotEmpty(inventory, inventory->activeSlot)) return NULL;
    return &inventory->slots[inventory->activeSlot];
}

const Weapon *Inventory_GetActiveWeaponConst(const Inventory *inventory)
{
    if (Inventory_IsSlotEmpty(inventory, inventory->activeSlot)) return NULL;
    return &inventory->slots[inventory->activeSlot];
}

bool Inventory_IsSwitching(const Inventory *inventory)
{
    return (inventory->pendingSlot != INVENTORY_NO_SLOT);
}

float Inventory_GetLowerAmount(const Inventory *inventory)
{
    if (!Inventory_IsSwitching(inventory)) return 0.0f;
    float t = inventory->switchTimer/inventory->switchDuration;     // 1 -> 0
    return 1.0f - fabsf(2.0f*t - 1.0f);                             // 0 -> 1 -> 0
}
