#include "world/SpatialGrid.h"
#include <algorithm>
#include <cmath>

void SpatialGrid::Configure(float worldWidth, float worldDepth, float cellSize)
{
    m_cellSize = cellSize;
    m_columns = std::max(1, static_cast<int>(std::ceil(worldWidth/cellSize)));
    m_rows = std::max(1, static_cast<int>(std::ceil(worldDepth/cellSize)));
    m_cells.assign(static_cast<size_t>(m_columns*m_rows), std::vector<GridEntry>());
}

void SpatialGrid::Clear()
{
    // clear() keeps each cell's capacity, so rebuilding every frame does not reallocate
    for (std::vector<GridEntry> &cell : m_cells) cell.clear();
}

int SpatialGrid::CellX(float x) const
{
    return std::clamp(static_cast<int>(std::floor(x/m_cellSize)), 0, m_columns - 1);
}

int SpatialGrid::CellZ(float z) const
{
    return std::clamp(static_cast<int>(std::floor(z/m_cellSize)), 0, m_rows - 1);
}

void SpatialGrid::Insert(std::uint64_t id, Vector3 position, float radius)
{
    if (m_cells.empty()) return;
    m_cells[static_cast<size_t>(CellZ(position.z)*m_columns + CellX(position.x))].push_back(GridEntry{ id, position, radius });
}
