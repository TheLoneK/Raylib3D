#pragma once

#include "raylib.h"
#include "items/PickupTypes.h"
#include "weapons/Weapon.h"
#include "world/Level.h"
#include <vector>

//----------------------------------------------------------------------------------
// Procedural level generator: rooms joined in a chain by corridors, optional
// locked doors along that chain, and spawn points for keys, switches, pickups
// and the boss. The game turns the spawn points into entities.
//----------------------------------------------------------------------------------
struct LevelGenSettings
{
    unsigned int seed = 0;
    int width = 40;                 // Grid size in cells
    int depth = 40;
    float tileSize = 4.0f;          // World units per cell
    float wallHeight = 5.0f;
    int minRooms = 7;
    int maxRooms = 11;
    int minRoomSize = 4;            // Room edge length in cells
    int maxRoomSize = 9;
    int pillarChance = 60;          // Percent chance a big room gets cover pillars
    int loopChance = 0;             // Percent chance of extra corridors (only inside an unlocked area)

    // Progression
    bool lockedDoors = false;       // Key / switch doors along the room chain
    int minRoomsPerLock = 3;
    int maxRoomsPerLock = 5;
    bool bossRoom = false;          // Boss in the last room, behind the last door

    // Pickups (per room, percent)
    int ammoChance = 50;
    int healthChance = 30;
    int damageBoostCount = 1;

    static LevelGenSettings Small(unsigned int seed);   // The original 40x40 arena, no doors
    static LevelGenSettings Large(unsigned int seed);   // ~3x the area, locked doors + boss
};

struct KeySpawn
{
    Vector3 position;
    int link;
};

struct SwitchSpawn
{
    Vector3 position;               // On the wall face
    Vector3 normal;                 // Points out of the wall, toward the player
    int link;
};

struct PickupSpawn
{
    Vector3 position;
    PickupType type;
    WeaponType weapon;              // PickupType::Ammo only
};

struct LevelFeatures
{
    Vector3 playerStart = { 0.0f, 0.0f, 0.0f };
    float playerYaw = 0.0f;
    std::vector<KeySpawn> keys;
    std::vector<SwitchSpawn> switches;
    std::vector<PickupSpawn> pickups;
    bool hasBoss = false;
    Vector3 bossPosition = { 0.0f, 0.0f, 0.0f };
};

namespace LevelGenerator
{
    LevelFeatures Generate(Level &level, const LevelGenSettings &settings);
}
