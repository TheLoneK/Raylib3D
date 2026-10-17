#pragma once

#include "raylib.h"
#include <vector>

//----------------------------------------------------------------------------------
// Grid based level: procedural rooms + corridors, collision and ray queries
//----------------------------------------------------------------------------------
// Add new tile kinds here (doors, half walls, hazards...) and handle them in Level.cpp
enum class TileType : unsigned char
{
    Empty = 0,
    Wall,
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
};

struct LevelGenSettings
{
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

    static LevelGenSettings Default(unsigned int seed);
};

class Level
{
public:
    // Generation
    void Generate(const LevelGenSettings &settings);

    // Grid queries (out of bounds counts as wall)
    TileType GetTile(int x, int z) const;
    bool IsSolid(int x, int z) const;
    bool IsSolid(CellCoord cell) const { return IsSolid(cell.x, cell.z); }
    bool IsSolidAt(Vector3 position) const;
    bool IsInside(int x, int z) const;
    CellCoord WorldToCell(Vector3 position) const;
    Vector3 CellToWorld(CellCoord cell) const;              // Cell center on the floor
    BoundingBox GetCellBounds(int x, int z) const;
    Vector3 GetRoomCenter(int roomIndex) const;
    Vector3 GetCenter() const;
    bool FindOpenCellNear(CellCoord center, int radius, CellCoord &outCell) const;

    // Collision: pushes a vertical cylinder (circle on XZ) out of walls.
    // Returns true on contact, outNormal (optional) gets the push direction
    bool ResolveCircle(Vector3 &position, float radius, Vector3 *outNormal = nullptr) const;

    // Ray queries against walls + floor. Ray direction does not need to be normalized
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
    void SetTile(int x, int z, TileType type);
    void CarveRoom(const LevelRoom &room);
    void CarveCorridor(CellCoord from, CellCoord to, bool horizontalFirst);
    void ConnectRooms(const LevelRoom &a, const LevelRoom &b);
    void PlacePillars(const LevelRoom &room);
    bool IsWallVisible(int x, int z) const;
    void DrawFloorCell(int x, int z) const;
    void DrawWallCell(int x, int z) const;

    int m_width = 0;
    int m_depth = 0;
    float m_tileSize = 4.0f;
    float m_wallHeight = 5.0f;
    unsigned int m_seed = 0;
    std::vector<TileType> m_tiles;      // Row major: index = z*width + x
    std::vector<LevelRoom> m_rooms;
};
