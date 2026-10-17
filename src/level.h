#ifndef LEVEL_H
#define LEVEL_H

#include "raylib.h"

//----------------------------------------------------------------------------------
// Grid based level: procedural rooms + corridors, collision and ray queries
//----------------------------------------------------------------------------------
#define LEVEL_MAX_WIDTH     64
#define LEVEL_MAX_DEPTH     64
#define LEVEL_MAX_ROOMS     24

// Add new tile kinds here (doors, half walls, hazards...) and handle them in level.c
typedef enum TileType {
    TILE_EMPTY = 0,
    TILE_WALL,
    TILE_TYPE_COUNT
} TileType;

typedef struct LevelRoom {
    int x;              // Top-left cell
    int z;
    int width;          // Size in cells
    int depth;
} LevelRoom;

typedef struct LevelGenSettings {
    unsigned int seed;
    int width;          // Grid size in cells
    int depth;
    float tileSize;     // World units per cell
    float wallHeight;
    int minRooms;
    int maxRooms;
    int minRoomSize;    // Room edge length in cells
    int maxRoomSize;
    int pillarChance;   // Percent chance a big room gets cover pillars
} LevelGenSettings;

typedef struct Level {
    int width;
    int depth;
    float tileSize;
    float wallHeight;
    unsigned int seed;
    unsigned char tiles[LEVEL_MAX_DEPTH][LEVEL_MAX_WIDTH];   // TileType per cell, indexed [z][x]
    LevelRoom rooms[LEVEL_MAX_ROOMS];
    int roomCount;
} Level;

// Generation
LevelGenSettings Level_DefaultSettings(unsigned int seed);
void Level_Generate(Level *level, LevelGenSettings settings);

// Grid queries (out of bounds counts as wall)
TileType Level_GetTile(const Level *level, int x, int z);
bool Level_IsSolid(const Level *level, int x, int z);
bool Level_IsSolidAtWorld(const Level *level, Vector3 position);
void Level_WorldToCell(const Level *level, Vector3 position, int *cellX, int *cellZ);
Vector3 Level_CellToWorld(const Level *level, int cellX, int cellZ);       // Cell center on the floor
BoundingBox Level_GetCellBounds(const Level *level, int cellX, int cellZ);
Vector3 Level_GetRoomCenter(const Level *level, int roomIndex);
Vector3 Level_GetCenter(const Level *level);
bool Level_FindOpenCellNear(const Level *level, int cellX, int cellZ, int radius, int *outX, int *outZ);

// Collision
// Pushes a vertical cylinder (circle on XZ) out of walls. Returns true on contact, outNormal (optional) gets the push direction
bool Level_ResolveCircle(const Level *level, Vector3 *position, float radius, Vector3 *outNormal);

// Ray queries: walls + floor. Ray direction does not need to be normalized
RayCollision Level_Raycast(const Level *level, Ray ray, float maxDistance);
bool Level_HasLineOfSight(const Level *level, Vector3 from, Vector3 to);

// Rendering
void Level_Draw(const Level *level);

#endif // LEVEL_H
