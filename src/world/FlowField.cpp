#include "world/FlowField.h"
#include <queue>
#include <functional>
#include <utility>

static constexpr float Unreachable = 1.0e9f;
static constexpr float DiagonalCost = 1.41421356f;

struct Neighbor
{
    int dx;
    int dz;
    float cost;
};

static constexpr Neighbor Neighbors[8] = {
    {  1,  0, 1.0f }, { -1,  0, 1.0f }, {  0,  1, 1.0f }, {  0, -1, 1.0f },
    {  1,  1, DiagonalCost }, { -1,  1, DiagonalCost }, {  1, -1, DiagonalCost }, { -1, -1, DiagonalCost },
};

// Diagonal steps are only allowed when both side cells are open (no cutting wall corners)
static bool CanStep(const Level &level, CellCoord from, const Neighbor &step)
{
    if (level.IsSolid(from.x + step.dx, from.z + step.dz)) return false;
    if ((step.dx != 0) && (step.dz != 0))
    {
        if (level.IsSolid(from.x + step.dx, from.z) || level.IsSolid(from.x, from.z + step.dz)) return false;
    }
    return true;
}

bool FlowField::Update(const Level &level, Vector3 targetPosition)
{
    CellCoord target = level.WorldToCell(targetPosition);
    bool sizeChanged = (m_width != level.GetWidth()) || (m_depth != level.GetDepth());
    if (m_valid && !sizeChanged && (target == m_target)) return false;

    Build(level, target);
    return true;
}

void FlowField::Build(const Level &level, CellCoord target)
{
    m_width = level.GetWidth();
    m_depth = level.GetDepth();
    m_target = target;
    m_valid = true;

    size_t cellCount = static_cast<size_t>(m_width*m_depth);
    m_distance.assign(cellCount, Unreachable);
    m_next.assign(cellCount, CellCoord{ -1, -1 });
    if (!level.IsInside(target.x, target.z) || level.IsSolid(target)) return;

    // Dijkstra outward from the target
    using Entry = std::pair<float, int>;   // cost, cell index
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    m_distance[static_cast<size_t>(Index(target))] = 0.0f;
    m_next[static_cast<size_t>(Index(target))] = target;
    open.push(Entry{ 0.0f, Index(target) });

    while (!open.empty())
    {
        auto [cost, index] = open.top();
        open.pop();
        if (cost > m_distance[static_cast<size_t>(index)]) continue;   // Stale entry

        CellCoord cell = { index%m_width, index/m_width };
        for (const Neighbor &step : Neighbors)
        {
            if (!CanStep(level, cell, step)) continue;

            CellCoord neighbor = { cell.x + step.dx, cell.z + step.dz };
            size_t neighborIndex = static_cast<size_t>(Index(neighbor));
            float newCost = cost + step.cost;
            if (newCost >= m_distance[neighborIndex]) continue;

            m_distance[neighborIndex] = newCost;
            m_next[neighborIndex] = cell;      // Walking from neighbor, step into this cell
            open.push(Entry{ newCost, Index(neighbor) });
        }
    }
}

float FlowField::GetDistance(CellCoord cell) const
{
    if (!m_valid || (cell.x < 0) || (cell.z < 0) || (cell.x >= m_width) || (cell.z >= m_depth)) return Unreachable;
    return m_distance[static_cast<size_t>(Index(cell))];
}

bool FlowField::GetNextWaypoint(const Level &level, Vector3 position, Vector3 &outWaypoint) const
{
    CellCoord cell = level.WorldToCell(position);
    if (GetDistance(cell) >= Unreachable) return false;

    CellCoord next = m_next[static_cast<size_t>(Index(cell))];
    outWaypoint = level.CellToWorld(next);
    return true;
}
