#pragma once

#include "raylib.h"
#include <vector>

//----------------------------------------------------------------------------------
// Grid based level: tiles, doors, collision and ray queries.
// LevelGenerator fills it in; gameplay code only reads it (and opens doors).
//----------------------------------------------------------------------------------
// Add new tile kinds here (half walls, hazards...) and handle them in Level.cpp
enum class TileType : unsigned char
{
    Empty = 0,
    Wall,
    Door,           // See Level::Door for its state
};

struct CellCoord
{
    int x;
    int z;

    bool operator==(const CellCoord &other) const { return (x == other.x) && (z == other.z); }
    bool operator!=(const CellCoord &other) const { return !(*this == other); }
};

struct LevelRoom
{
    int x;              // Top-left cell
    int z;
    int width;          // Size in cells
    int depth;

    CellCoord Center() const { return CellCoord{ x + width/2, z + depth/2 }; }
    bool Contains(CellCoord cell) const { return (cell.x >= x) && (cell.x < x + width) && (cell.z >= z) && (cell.z < z + depth); }
};

enum class DoorLock
{
    Key,            // Opens when the player walks up holding the key with the same link
    Switch          // Opens when a wall switch with the same link is used
};

class Level
{
public:
    struct Door
    {
        CellCoord cell;
        bool spansX;            // Slab runs along X (the corridor runs along Z)
        int link;               // Link tag shared with its key / switch
        DoorLock lock;
        bool opening;           // Unlocked: slides open over time
        float openAmount;       // 0 closed .. 1 open

        bool IsBlocking() const { return openAmount < 0.6f; }
    };

    static constexpr float DoorThickness = 0.6f;
    static constexpr float DoorOpenTime = 1.2f;

    // Building (used by LevelGenerator)
    void Reset(int width, int depth, float tileSize, float wallHeight, unsigned int seed);
    void SetTile(int x, int z, TileType type);      // Never changes the outer border
    void AddRoom(const LevelRoom &room) { m_rooms.push_back(room); }
    void AddDoor(CellCoord cell, bool spansX, int link, DoorLock lock);
    void RemoveDoor(CellCoord cell);                // Turns the cell back into floor

    // Grid queries (out of bounds counts as wall)
    TileType GetTile(int x, int z) const;
    bool IsWall(int x, int z) const;                // Static geometry only
    bool IsSolid(int x, int z) const;               // Blocks movement right now: walls + closed doors
    bool IsSolid(CellCoord cell) const { return IsSolid(cell.x, cell.z); }
    bool IsSolidAt(Vector3 position) const;
    bool IsInside(int x, int z) const;
    CellCoord WorldToCell(Vector3 position) const;
    Vector3 CellToWorld(CellCoord cell) const;      // Cell center on the floor
    BoundingBox GetCellBounds(int x, int z) const;
    Vector3 GetRoomCenter(int roomIndex) const;
    Vector3 GetCenter() const;
    bool FindOpenCellNear(CellCoord center, int radius, CellCoord &outCell) const;

    // Doors
    const std::vector<Door> &GetDoors() const { return m_doors; }
    const Door *FindDoor(CellCoord cell) const;
    Vector3 GetDoorCenter(const Door &door) const;
    BoundingBox GetDoorBounds(const Door &door) const;
    void OpenDoor(int doorIndex);
    int OpenDoorsWithLink(int link);                // Returns how many started opening
    void UpdateDoors(float deltaTime);
    int GetNavigationVersion() const { return m_navigationVersion; }    // Changes whenever a door starts/stops blocking

    // Collision: pushes a vertical cylinder (circle on XZ) out of walls and closed doors.
    // Returns true on contact, outNormal (optional) gets the push direction
    bool ResolveCircle(Vector3 &position, float radius, Vector3 *outNormal = nullptr) const;

    // Ray queries against walls, doors and the floor. Ray direction does not need to be normalized
    RayCollision Raycast(Ray ray, float maxDistance) const;
    bool HasLineOfSight(Vector3 from, Vector3 to) const;

    void Draw() const;

    int GetWidth() const { return m_width; }
    int GetDepth() const { return m_depth; }
    float GetTileSize() const { return m_tileSize; }
    float GetWallHeight() const { return m_wallHeight; }
    unsigned int GetSeed() const { return m_seed; }
    const std::vector<LevelRoom> &GetRooms() const { return m_rooms; }

private:
    int Index(int x, int z) const { return z*m_width + x; }
    bool IsWallVisible(int x, int z) const;
    bool GetBlockingRect(int x, int z, Rectangle &outRect) const;  // XZ footprint of whatever blocks this cell
    void DrawFloorCell(int x, int z) const;
    void DrawWallCell(int x, int z) const;
    void DrawDoor(const Door &door) const;

    int m_width = 0;
    int m_depth = 0;
    float m_tileSize = 4.0f;
    float m_wallHeight = 5.0f;
    unsigned int m_seed = 0;
    int m_navigationVersion = 0;
    std::vector<TileType> m_tiles;      // Row major: index = z*width + x
    std::vector<int> m_doorIndex;       // Per cell: index into m_doors or -1
    std::vector<LevelRoom> m_rooms;
    std::vector<Door> m_doors;
};
