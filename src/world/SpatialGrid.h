#pragma once

#include "raylib.h"
#include <cstdint>
#include <vector>

//----------------------------------------------------------------------------------
// Uniform grid for "who is near this point" queries (crowd separation, splash hits...).
// Rebuilt every frame: Clear() then Insert() everything. Entries carry an opaque id
// (the ECS stores entity ids here).
//----------------------------------------------------------------------------------
struct GridEntry
{
    std::uint64_t id;
    Vector3 position;
    float radius;
};

class SpatialGrid
{
public:
    void Configure(float worldWidth, float worldDepth, float cellSize);
    void Clear();
    void Insert(std::uint64_t id, Vector3 position, float radius);

    // Calls visitor(const GridEntry &) for every entry in cells overlapping the circle
    template <typename Visitor>
    void QueryRadius(Vector3 center, float radius, Visitor &&visitor) const
    {
        int minX = CellX(center.x - radius), maxX = CellX(center.x + radius);
        int minZ = CellZ(center.z - radius), maxZ = CellZ(center.z + radius);

        for (int z = minZ; z <= maxZ; z++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                for (const GridEntry &entry : m_cells[static_cast<size_t>(z*m_columns + x)]) visitor(entry);
            }
        }
    }

private:
    int CellX(float x) const;
    int CellZ(float z) const;

    float m_cellSize = 1.0f;
    int m_columns = 0;
    int m_rows = 0;
    std::vector<std::vector<GridEntry>> m_cells;
};
