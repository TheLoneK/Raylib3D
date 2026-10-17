#include "world/Level.h"
#include "core/Render.h"
#include "world/LinkTags.h"
#include "raymath.h"
#include <algorithm>
#include <cstddef>

//----------------------------------------------------------------------------------
// Building
//----------------------------------------------------------------------------------
void Level::Reset(int width, int depth, float tileSize, float wallHeight, unsigned int seed)
{
    m_width = std::max(width, 8);
    m_depth = std::max(depth, 8);
    m_tileSize = tileSize;
    m_wallHeight = wallHeight;
    m_seed = seed;
    m_navigationVersion++;
    m_tiles.assign(static_cast<size_t>(m_width*m_depth), TileType::Wall);
    m_doorIndex.assign(static_cast<size_t>(m_width*m_depth), -1);
    m_rooms.clear();
    m_doors.clear();
}

void Level::SetTile(int x, int z, TileType type)
{
    // Never carve the outer border so the level is always closed
    if ((x <= 0) || (z <= 0) || (x >= m_width - 1) || (z >= m_depth - 1)) return;
    m_tiles[static_cast<size_t>(Index(x, z))] = type;
}

void Level::AddDoor(CellCoord cell, bool spansX, int link, DoorLock lock)
{
    if (!IsInside(cell.x, cell.z) || (FindDoor(cell) != nullptr)) return;

    SetTile(cell.x, cell.z, TileType::Door);
    m_doorIndex[static_cast<size_t>(Index(cell.x, cell.z))] = static_cast<int>(m_doors.size());
    m_doors.push_back(Door{ cell, spansX, link, lock, false, 0.0f });
}

void Level::RemoveDoor(CellCoord cell)
{
    const Door *door = FindDoor(cell);
    if (door == nullptr) return;

    size_t removed = static_cast<size_t>(door - m_doors.data());
    m_doors.erase(m_doors.begin() + static_cast<std::ptrdiff_t>(removed));
    SetTile(cell.x, cell.z, TileType::Empty);

    // Rebuild the lookup since indices shifted
    std::fill(m_doorIndex.begin(), m_doorIndex.end(), -1);
    for (size_t i = 0; i < m_doors.size(); i++) m_doorIndex[static_cast<size_t>(Index(m_doors[i].cell.x, m_doors[i].cell.z))] = static_cast<int>(i);
}

//----------------------------------------------------------------------------------
// Grid queries
//----------------------------------------------------------------------------------
bool Level::IsInside(int x, int z) const
{
    return (x >= 0) && (z >= 0) && (x < m_width) && (z < m_depth);
}

TileType Level::GetTile(int x, int z) const
{
    if (!IsInside(x, z)) return TileType::Wall;
    return m_tiles[static_cast<size_t>(Index(x, z))];
}

bool Level::IsWall(int x, int z) const
{
    return (GetTile(x, z) == TileType::Wall);
}

bool Level::IsSolid(int x, int z) const
{
    switch (GetTile(x, z))
    {
        case TileType::Wall: return true;
        case TileType::Door: return FindDoor(CellCoord{ x, z })->IsBlocking();
        default: return false;
    }
}

CellCoord Level::WorldToCell(Vector3 position) const
{
    return CellCoord{ static_cast<int>(floorf(position.x/m_tileSize)), static_cast<int>(floorf(position.z/m_tileSize)) };
}

bool Level::IsSolidAt(Vector3 position) const
{
    return IsSolid(WorldToCell(position));
}

Vector3 Level::CellToWorld(CellCoord cell) const
{
    return Vector3{ (static_cast<float>(cell.x) + 0.5f)*m_tileSize, 0.0f, (static_cast<float>(cell.z) + 0.5f)*m_tileSize };
}

BoundingBox Level::GetCellBounds(int x, int z) const
{
    return BoundingBox{
        Vector3{ static_cast<float>(x)*m_tileSize, 0.0f, static_cast<float>(z)*m_tileSize },
        Vector3{ static_cast<float>(x + 1)*m_tileSize, m_wallHeight, static_cast<float>(z + 1)*m_tileSize },
    };
}

//----------------------------------------------------------------------------------
// Doors
//----------------------------------------------------------------------------------
const Level::Door *Level::FindDoor(CellCoord cell) const
{
    if (!IsInside(cell.x, cell.z)) return nullptr;
    int index = m_doorIndex[static_cast<size_t>(Index(cell.x, cell.z))];
    return (index >= 0) ? &m_doors[static_cast<size_t>(index)] : nullptr;
}

Vector3 Level::GetDoorCenter(const Door &door) const
{
    return CellToWorld(door.cell) + Vector3{ 0.0f, m_wallHeight*0.5f, 0.0f };
}

BoundingBox Level::GetDoorBounds(const Door &door) const
{
    // The slab rolls up into the ceiling as it opens
    Vector3 center = CellToWorld(door.cell);
    float half = m_tileSize*0.5f;
    float thickness = DoorThickness*0.5f;
    float bottom = door.openAmount*(m_wallHeight - 0.25f);

    if (door.spansX)
    {
        return BoundingBox{ Vector3{ center.x - half, bottom, center.z - thickness }, Vector3{ center.x + half, m_wallHeight, center.z + thickness } };
    }
    return BoundingBox{ Vector3{ center.x - thickness, bottom, center.z - half }, Vector3{ center.x + thickness, m_wallHeight, center.z + half } };
}

void Level::OpenDoor(int doorIndex)
{
    if ((doorIndex >= 0) && (doorIndex < static_cast<int>(m_doors.size()))) m_doors[static_cast<size_t>(doorIndex)].opening = true;
}

int Level::OpenDoorsWithLink(int link)
{
    int opened = 0;
    for (Door &door : m_doors)
    {
        if ((door.link == link) && !door.opening)
        {
            door.opening = true;
            opened++;
        }
    }
    return opened;
}

void Level::UpdateDoors(float deltaTime)
{
    for (Door &door : m_doors)
    {
        if (!door.opening || (door.openAmount >= 1.0f)) continue;

        bool wasBlocking = door.IsBlocking();
        door.openAmount = std::min(1.0f, door.openAmount + deltaTime/DoorOpenTime);
        if (wasBlocking != door.IsBlocking()) m_navigationVersion++;     // Pathfinding needs a rebuild
    }
}

Vector3 Level::GetRoomCenter(int roomIndex) const
{
    return CellToWorld(m_rooms[static_cast<size_t>(roomIndex)].Center());
}

Vector3 Level::GetCenter() const
{
    return Vector3{ static_cast<float>(m_width)*m_tileSize*0.5f, 0.0f, static_cast<float>(m_depth)*m_tileSize*0.5f };
}

bool Level::FindOpenCellNear(CellCoord center, int radius, CellCoord &outCell) const
{
    for (int attempt = 0; attempt < 24; attempt++)
    {
        CellCoord cell = { center.x + GetRandomValue(-radius, radius), center.z + GetRandomValue(-radius, radius) };
        if (!IsSolid(cell))
        {
            outCell = cell;
            return true;
        }
    }
    return false;
}

//----------------------------------------------------------------------------------
// Collision
//----------------------------------------------------------------------------------
bool Level::GetBlockingRect(int x, int z, Rectangle &outRect) const
{
    switch (GetTile(x, z))
    {
        case TileType::Wall:
        {
            outRect = Rectangle{ static_cast<float>(x)*m_tileSize, static_cast<float>(z)*m_tileSize, m_tileSize, m_tileSize };
            return true;
        }
        case TileType::Door:
        {
            const Door *door = FindDoor(CellCoord{ x, z });
            if (!door->IsBlocking()) return false;
            BoundingBox box = GetDoorBounds(*door);
            outRect = Rectangle{ box.min.x, box.min.z, box.max.x - box.min.x, box.max.z - box.min.z };
            return true;
        }
        default: return false;
    }
}

bool Level::ResolveCircle(Vector3 &position, float radius, Vector3 *outNormal) const
{
    bool collided = false;
    Vector3 totalPush = { 0.0f, 0.0f, 0.0f };

    // A few passes handle corners where two walls push at once
    for (int pass = 0; pass < 3; pass++)
    {
        bool pushedThisPass = false;
        CellCoord center = WorldToCell(position);

        for (int z = center.z - 1; z <= center.z + 1; z++)
        {
            for (int x = center.x - 1; x <= center.x + 1; x++)
            {
                Rectangle rect;
                if (!GetBlockingRect(x, z, rect)) continue;

                float minX = rect.x, maxX = rect.x + rect.width;
                float minZ = rect.y, maxZ = rect.y + rect.height;

                // Closest point on the wall rectangle to the circle center
                float dx = position.x - Clamp(position.x, minX, maxX);
                float dz = position.z - Clamp(position.z, minZ, maxZ);
                float distSq = dx*dx + dz*dz;
                if (distSq >= radius*radius) continue;

                Vector3 push = { 0.0f, 0.0f, 0.0f };
                if (distSq > 0.000001f)
                {
                    float dist = sqrtf(distSq);
                    float depth = radius - dist;
                    push = Vector3{ dx/dist*depth, 0.0f, dz/dist*depth };
                }
                else
                {
                    // Center is inside the wall: leave through the closest face
                    float left = position.x - minX, right = maxX - position.x;
                    float top = position.z - minZ, bottom = maxZ - position.z;
                    float best = std::min({ left, right, top, bottom });
                    if (best == left) push.x = -(left + radius);
                    else if (best == right) push.x = right + radius;
                    else if (best == top) push.z = -(top + radius);
                    else push.z = bottom + radius;
                }

                position += push;
                totalPush += push;
                collided = true;
                pushedThisPass = true;
            }
        }

        if (!pushedThisPass) break;
    }

    if (outNormal != nullptr) *outNormal = collided ? Vector3Normalize(totalPush) : Vector3{ 0.0f, 0.0f, 0.0f };
    return collided;
}

//----------------------------------------------------------------------------------
// Ray queries
//----------------------------------------------------------------------------------
RayCollision Level::Raycast(Ray ray, float maxDistance) const
{
    ray.direction = Vector3Normalize(ray.direction);
    Vector3 dir = ray.direction;

    RayCollision result = {};
    result.distance = maxDistance;

    // Floor plane at y = 0
    if (dir.y < -0.0001f)
    {
        float t = -ray.position.y/dir.y;
        if ((t >= 0.0f) && (t < result.distance))
        {
            result.hit = true;
            result.distance = t;
            result.normal = Vector3{ 0.0f, 1.0f, 0.0f };
        }
    }

    // Walk the grid cells the ray crosses (DDA) and test walls in order
    CellCoord cell = WorldToCell(ray.position);
    int stepX = (dir.x > 0.0f) ? 1 : -1;
    int stepZ = (dir.z > 0.0f) ? 1 : -1;
    const float huge = 1.0e30f;

    float tDeltaX = (fabsf(dir.x) > 0.000001f) ? m_tileSize/fabsf(dir.x) : huge;
    float tDeltaZ = (fabsf(dir.z) > 0.000001f) ? m_tileSize/fabsf(dir.z) : huge;
    float boundaryX = static_cast<float>((stepX > 0) ? cell.x + 1 : cell.x)*m_tileSize;
    float boundaryZ = static_cast<float>((stepZ > 0) ? cell.z + 1 : cell.z)*m_tileSize;
    float tMaxX = (fabsf(dir.x) > 0.000001f) ? (boundaryX - ray.position.x)/dir.x : huge;
    float tMaxZ = (fabsf(dir.z) > 0.000001f) ? (boundaryZ - ray.position.z)/dir.z : huge;
    float tCurrent = 0.0f;

    while (tCurrent <= result.distance)
    {
        if (!IsInside(cell.x, cell.z)) break;

        TileType tile = GetTile(cell.x, cell.z);
        if (tile != TileType::Empty)
        {
            // Doors are thin and may be partly open, so a ray can pass beside or under them
            BoundingBox box = (tile == TileType::Wall) ? GetCellBounds(cell.x, cell.z) : GetDoorBounds(*FindDoor(cell));
            RayCollision hit = GetRayCollisionBox(ray, box);
            if (hit.hit && (hit.distance < result.distance))
            {
                result = hit;
                break;
            }
        }

        if (tMaxX < tMaxZ)
        {
            tCurrent = tMaxX;
            tMaxX += tDeltaX;
            cell.x += stepX;
        }
        else
        {
            tCurrent = tMaxZ;
            tMaxZ += tDeltaZ;
            cell.z += stepZ;
        }
    }

    result.point = ray.position + dir*result.distance;
    return result;
}

bool Level::HasLineOfSight(Vector3 from, Vector3 to) const
{
    Vector3 delta = to - from;
    float distance = Vector3Length(delta);
    if (distance < 0.0001f) return true;

    return !Raycast(Ray{ from, delta }, distance).hit;
}

//----------------------------------------------------------------------------------
// Rendering
//----------------------------------------------------------------------------------
static Color GetWallColor(int x, int z)
{
    // Cheap per-cell variation so walls do not look like one flat block
    unsigned int hash = (static_cast<unsigned int>(x)*73856093u) ^ (static_cast<unsigned int>(z)*19349663u);
    float variation = 0.9f + static_cast<float>(hash%16u)*0.012f;
    return Render::Shade(Color{ 120, 112, 130, 255 }, variation);
}

bool Level::IsWallVisible(int x, int z) const
{
    return !IsWall(x + 1, z) || !IsWall(x - 1, z) || !IsWall(x, z + 1) || !IsWall(x, z - 1);
}

void Level::DrawFloorCell(int x, int z) const
{
    Vector3 origin = Vector3{ static_cast<float>(x)*m_tileSize, 0.0f, static_cast<float>(z)*m_tileSize };
    Color base = ((x + z) & 1) ? Color{ 92, 96, 104, 255 } : Color{ 76, 80, 88, 255 };
    Color color = Render::ApplyFog(base, CellToWorld(CellCoord{ x, z }));

    Render::DrawQuad3D(origin, Vector3{ 0.0f, 0.0f, m_tileSize }, Vector3{ m_tileSize, 0.0f, 0.0f }, color);
}

void Level::DrawWallCell(int x, int z) const
{
    float x0 = static_cast<float>(x)*m_tileSize, x1 = x0 + m_tileSize;
    float z0 = static_cast<float>(z)*m_tileSize, z1 = z0 + m_tileSize;
    float half = m_tileSize*0.5f;
    float midY = m_wallHeight*0.5f;
    Color base = GetWallColor(x, z);
    Vector3 up = Vector3{ 0.0f, m_wallHeight, 0.0f };
    Vector3 alongX = Vector3{ m_tileSize, 0.0f, 0.0f };
    Vector3 alongZ = Vector3{ 0.0f, 0.0f, m_tileSize };

    // Only faces next to open space are visible; each direction gets its own shade for depth
    if (!IsWall(x + 1, z)) Render::DrawQuad3D(Vector3{ x1, 0.0f, z0 }, up, alongZ, Render::ApplyFog(Render::Shade(base, 0.85f), Vector3{ x1, midY, z0 + half }));
    if (!IsWall(x - 1, z)) Render::DrawQuad3D(Vector3{ x0, 0.0f, z0 }, alongZ, up, Render::ApplyFog(Render::Shade(base, 0.65f), Vector3{ x0, midY, z0 + half }));
    if (!IsWall(x, z + 1)) Render::DrawQuad3D(Vector3{ x0, 0.0f, z1 }, alongX, up, Render::ApplyFog(Render::Shade(base, 1.00f), Vector3{ x0 + half, midY, z1 }));
    if (!IsWall(x, z - 1)) Render::DrawQuad3D(Vector3{ x0, 0.0f, z0 }, up, alongX, Render::ApplyFog(Render::Shade(base, 0.75f), Vector3{ x0 + half, midY, z0 }));

    // Top cap (visible while jumping)
    Color top = Render::ApplyFog(Render::Shade(base, 1.15f), Vector3{ x0 + half, m_wallHeight, z0 + half });
    Render::DrawQuad3D(Vector3{ x0, m_wallHeight, z0 }, alongZ, alongX, top);
}

void Level::DrawDoor(const Door &door) const
{
    BoundingBox box = GetDoorBounds(door);
    Vector3 size = box.max - box.min;
    Vector3 center = (box.min + box.max)*0.5f;
    if (size.y < 0.05f) return;

    Color linkColor = LinkTags::GetColor(door.link);
    Color slab = Render::ApplyFog(Render::Shade(Color{ 110, 104, 96, 255 }, 0.9f), center);
    DrawCubeV(center, size, slab);
    DrawCubeWiresV(center, size, Render::ApplyFog(Render::Shade(linkColor, 0.5f), center));

    // Colored band (key doors) or two bands (switch doors) so the link is readable from both sides
    Vector3 bandSize = door.spansX ? Vector3{ size.x*0.9f, 0.35f, size.z + 0.04f } : Vector3{ size.x + 0.04f, 0.35f, size.z*0.9f };
    float bandY = std::max(box.min.y + 0.4f, m_wallHeight*0.45f);
    if (bandY + 0.2f < box.max.y) DrawCubeV(Vector3{ center.x, bandY, center.z }, bandSize, Render::ApplyFog(linkColor, center));
    if ((door.lock == DoorLock::Switch) && (bandY + 1.0f < box.max.y)) DrawCubeV(Vector3{ center.x, bandY + 0.8f, center.z }, bandSize, Render::ApplyFog(linkColor, center));
}

void Level::Draw() const
{
    for (int z = 0; z < m_depth; z++)
    {
        for (int x = 0; x < m_width; x++)
        {
            // Skip cells the fog hides completely; the big level has thousands of them
            if (Render::IsFullyFogged(CellToWorld(CellCoord{ x, z }), m_tileSize)) continue;

            if (!IsWall(x, z)) DrawFloorCell(x, z);
            else if (IsWallVisible(x, z)) DrawWallCell(x, z);
        }
    }

    for (const Door &door : m_doors)
    {
        if (!Render::IsFullyFogged(GetDoorCenter(door), m_tileSize)) DrawDoor(door);
    }
}
