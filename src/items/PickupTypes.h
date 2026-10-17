#pragma once

//----------------------------------------------------------------------------------
// Pickup kinds. Kept in its own tiny header so the level generator can place
// pickups without depending on the pickup systems.
//----------------------------------------------------------------------------------
enum class PickupType
{
    Ammo,           // Reserve ammo for one weapon (Pickup::weapon)
    Health,
    DamageBoost,    // 2x damage for a while
    Key             // Opens doors with the same link tag (Pickup::link)
};
