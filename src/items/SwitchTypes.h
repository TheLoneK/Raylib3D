#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Wall switch data shared by the level generator (which places them) and
// Interactions (which spawns and uses them). No ECS here.
//----------------------------------------------------------------------------------
enum class SwitchMode
{
    Once,           // Stays down after one use (door switches)
    Toggle          // Flips on every press (platform switches)
};

struct SwitchSpawn
{
    Vector3 position;               // On the wall face
    Vector3 normal;                 // Points out of the wall, toward the player
    int channel;                    // Trigger channel it sends on (triggers/Triggers.h)
    SwitchMode mode;
    Color color;
    const char *name;               // Static string, e.g. "RED" -> "[E] Use RED switch"
};
