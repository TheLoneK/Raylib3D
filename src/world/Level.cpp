#include "world/Level.h"
#include "core/Render.h"
#include "raymath.h"
#include <algorithm>

//----------------------------------------------------------------------------------
// Generation
//----------------------------------------------------------------------------------
LevelGenSettings LevelGenSettings::Default(unsigned int seed)
{
    LevelGenSettings settings = {};
    settings.seed = seed;
    settings.width = 40;
    settings.depth = 40;
    settings.tileSize = 4.0f;
    settings.wallHeight = 5.0f;
    settings.minRooms = 7;
    settings.maxRooms = 11;
    settings.minRoomSize = 4;
    settings.maxRoomSize = 9;
    settings.pillarChance = 60;
    return settings;
}

void Level::SetTile(int x, int z, TileType type)
{
    // Never carve the outer border so the level is always closed
    if ((x <= 0) || (z <= 0) || (x >= m_width - 1) || (z >= m_depth - 1)) return;
    m_tiles[static_cast<size_t>(z*m_width + x)] = type;
}

void Level::CarveRoom(const LevelRoom &room)
{
    for (int z = room.z; z < room.z + room.depth; z++)
    {
        for (int x = room.x; x < room.x + room.width; x++) SetTile(x, z, TileType::Empty);
    }
}

void Level::CarveCorridor(CellCoord from, CellCoord to, bool horizontalFirst)
{
    int stepX = (to.x > from.x) ? 1 : -1;
    int stepZ = (to.z > from.z) ? 1 : -1;

    if (horizontalFirst)
    {
        for (int x = from.x; x != to.x + stepX; x += stepX) SetTile(x, from.z, TileType::Empty);
        for (int z = from.z; z != to.z + stepZ; z += stepZ) SetTile(to.x, z, TileType::Empty);
    }
    else
    {
        for (int z = from.z; z != to.z + stepZ; z += stepZ) SetTile(from.x, z, TileType::Empty);
        for (int x = from.x; x != to.x + stepX; x += stepX) SetTile(x, to.z, TileType::Empty);
    }
}

static bool RoomsOverlap(const LevelRoom &a, const LevelRoom &b, int padding)
{
    return (a.x - padding < b.x + b.width) && (a.x + a.width + padding > b.x) &&
           (a.z - padding < b.z + b.depth) && (a.z + a.depth + padding > b.z);
}

void Level::ConnectRooms(const LevelRoom &a, const LevelRoom &b)
{
    CarveCorridor(a.Center(), b.Center(), (GetRandomValue(0, 1) == 1));
}

void Level::PlacePillars(const LevelRoom &room)
{
    // Pillars sit two cells in from each corner so the room stays connected around them
    if ((room.width < 7) || (room.depth < 7)) return;

    int left = room.x + 2;
    int right = room.x + room.width - 3;
    int top = room.z + 2;
    int bottom = room.z + room.depth - 3;

    SetTile(left, top, TileType::Wall);
    SetTile(right, top, TileType::Wall);
    SetTile(left, bottom, TileType::Wall);
    SetTile(right, bottom, TileType::Wall);
}

void Level::Generate(const LevelGenSettings &settings)
{
    m_width = std::max(settings.width, 8);
    m_depth = std::max(settings.depth, 8);
    m_tileSize = settings.tileSize;
    m_wallHeight = settings.wallHeight;
    m_seed = settings.seed;
    m_rooms.clear();

    SetRandomSeed(settings.seed);

    // Start fully solid, then carve rooms out of it
    m_tiles.assign(static_cast<size_t>(m_width*m_depth), TileType::Wall);

    int targetRooms = GetRandomValue(settings.minRooms, settings.maxRooms);

    for (int attempt = 0; (attempt < 400) && (static_cast<int>(m_rooms.size()) < targetRooms); attempt++)
    {
        LevelRoom room = {};
        room.width = GetRandomValue(settings.minRoomSize, settings.maxRoomSize);
        room.depth = GetRandomValue(settings.minRoomSize, settings.maxRoomSize);
        if ((room.width >= m_width - 2) || (room.depth >= m_depth - 2)) continue;

        room.x = GetRandomValue(1, m_width - room.width - 1);
        room.z = GetRandomValue(1, m_depth - room.depth - 1);

        bool overlaps = std::any_of(m_rooms.begin(), m_rooms.end(), [&](const LevelRoom &other) { return RoomsOverlap(room, other, 2); });
        if (overlaps) continue;

        CarveRoom(room);
        if (!m_rooms.empty()) ConnectRooms(m_rooms.back(), room);
        m_rooms.push_back(room);
    }

    // One extra connection creates a loop so fights are not purely linear
    if (m_rooms.size() > 3) ConnectRooms(m_rooms.front(), m_rooms.back());

    for (const LevelRoom &room : m_rooms)
    {
        if (GetRandomValue(0, 99) < settings.pillarChance) PlacePillars(room);
    }
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
    return m_tiles[static_cast<size_t>(z*m_width + x)];
}

bool Level::IsSolid(int x, int z) const
{
    return (GetTile(x, z) == TileType::Wall);
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
                if (!IsSolid(x, z)) continue;

                float minX = static_cast<float>(x)*m_tileSize, maxX = minX + m_tileSize;
                float minZ = static_cast<float>(z)*m_tileSize, maxZ = minZ + m_tileSize;

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

        if (IsSolid(cell))
        {
            RayCollision wallHit = GetRayCollisionBox(ray, GetCellBounds(cell.x, cell.z));
            if (wallHit.hit && (wallHit.distance < result.distance))
            {
                result = wallHit;
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
    return !IsSolid(x + 1, z) || !IsSolid(x - 1, z) || !IsSolid(x, z + 1) || !IsSolid(x, z - 1);
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
    if (!IsSolid(x + 1, z)) Render::DrawQuad3D(Vector3{ x1, 0.0f, z0 }, up, alongZ, Render::ApplyFog(Render::Shade(base, 0.85f), Vector3{ x1, midY, z0 + half }));
    if (!IsSolid(x - 1, z)) Render::DrawQuad3D(Vector3{ x0, 0.0f, z0 }, alongZ, up, Render::ApplyFog(Render::Shade(base, 0.65f), Vector3{ x0, midY, z0 + half }));
    if (!IsSolid(x, z + 1)) Render::DrawQuad3D(Vector3{ x0, 0.0f, z1 }, alongX, up, Render::ApplyFog(Render::Shade(base, 1.00f), Vector3{ x0 + half, midY, z1 }));
    if (!IsSolid(x, z - 1)) Render::DrawQuad3D(Vector3{ x0, 0.0f, z0 }, up, alongX, Render::ApplyFog(Render::Shade(base, 0.75f), Vector3{ x0 + half, midY, z0 }));

    // Top cap (visible while jumping)
    Color top = Render::ApplyFog(Render::Shade(base, 1.15f), Vector3{ x0 + half, m_wallHeight, z0 + half });
    Render::DrawQuad3D(Vector3{ x0, m_wallHeight, z0 }, alongZ, alongX, top);
}

void Level::Draw() const
{
    for (int z = 0; z < m_depth; z++)
    {
        for (int x = 0; x < m_width; x++)
        {
            if (!IsSolid(x, z)) DrawFloorCell(x, z);
            else if (IsWallVisible(x, z)) DrawWallCell(x, z);
        }
    }
}
