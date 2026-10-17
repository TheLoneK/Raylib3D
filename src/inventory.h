#ifndef INVENTORY_H
#define INVENTORY_H

#include "weapon.h"

//----------------------------------------------------------------------------------
// Weapon inventory: fixed slots selected with keys 1-7 / mouse wheel
//----------------------------------------------------------------------------------
#define INVENTORY_SLOT_COUNT    7
#define INVENTORY_NO_SLOT       (-1)

typedef struct Inventory {
    Weapon slots[INVENTORY_SLOT_COUNT];     // type == WEAPON_NONE means the slot is empty
    int activeSlot;
    int previousSlot;
    int pendingSlot;                        // Slot being switched to, INVENTORY_NO_SLOT when idle
    float switchTimer;                      // Counts down while switching
    float switchDuration;
} Inventory;

void Inventory_Init(Inventory *inventory);
int Inventory_GiveWeapon(Inventory *inventory, WeaponType type);          // First free slot (or adds ammo if owned). Returns slot or INVENTORY_NO_SLOT
bool Inventory_SetSlot(Inventory *inventory, int slot, WeaponType type);  // Place a weapon in a specific slot
bool Inventory_IsSlotEmpty(const Inventory *inventory, int slot);

bool Inventory_SelectSlot(Inventory *inventory, int slot);
void Inventory_SelectNext(Inventory *inventory, int direction);           // +1 / -1, skips empty slots
void Inventory_SelectPrevious(Inventory *inventory);                      // Swap to last used weapon

void Inventory_Update(Inventory *inventory, float deltaTime);
Weapon *Inventory_GetActiveWeapon(Inventory *inventory);                  // NULL when the active slot is empty
const Weapon *Inventory_GetActiveWeaponConst(const Inventory *inventory);
bool Inventory_IsSwitching(const Inventory *inventory);
float Inventory_GetLowerAmount(const Inventory *inventory);               // 0 = raised, 1 = fully lowered (viewmodel)

#endif // INVENTORY_H
