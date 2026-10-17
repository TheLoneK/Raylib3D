#pragma once

#include "world/Level.h"
#include <vector>

//----------------------------------------------------------------------------------
// Flow field: one shortest-path search from the target (the player) to every cell.
// Any number of agents can then path to the target by looking up their own cell,
// which is what makes hundreds of chasing enemies cheap.
//----------------------------------------------------------------------------------
class FlowField
{
public:
    // Rebuilds only when the target moves to a different cell. Returns true if it rebuilt
    bool Update(const Level &level, Vector3 targetPosition);
    void Build(const Level &level, CellCoord target);

    // World position of the next cell to walk toward. Returns false if the target is unreachable
    bool GetNextWaypoint(const Level &level, Vector3 position, Vector3 &outWaypoint) const;
    float GetDistance(CellCoord cell) const;    // Path cost to the target (cells), huge if unreachable
    CellCoord GetTarget() const { return m_target; }

private:
    int Index(CellCoord cell) const { return cell.z*m_width + cell.x; }

    int m_width = 0;
    int m_depth = 0;
    bool m_valid = false;
    CellCoord m_target = { -1, -1 };
    std::vector<float> m_distance;      // Per cell path cost to the target
    std::vector<CellCoord> m_next;      // Per cell: neighbor to step into
};
