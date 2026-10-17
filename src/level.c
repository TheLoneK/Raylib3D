#include "level.h"
#include "render.h"
#include "raymath.h"
#include <stddef.h>
#include <string.h>

//----------------------------------------------------------------------------------
// Generation
//----------------------------------------------------------------------------------
LevelGenSettings Level_DefaultSettings(unsigned int seed)
{
    return (LevelGenSettings){
        .seed = seed,
        .width = 40,
        .depth = 40,
        .tileSize = 4.0f,
        .wallHeight = 5.0f,
        .minRooms = 7,
        .maxRooms = 11,
        .minRoomSize = 4,
        .maxRoomSize = 9,
        .pillarChance = 60,
    };
}

static void SetTile(Level *level, int x, int z, TileType type)
{
    // Never carve the outer border so the level is always closed
    if ((x <= 0) || (z <= 0) || (x >= level->width - 1) || (z >= level->depth - 1)) return;
    level->tiles[z][x] = (unsigned char)type;
}

static void CarveRoom(Level *level, LevelRoom room)
{
    for (int z = room.z; z < room.z + room.depth; z++)
    {
        for (int x = room.x; x < room.x + room.width; x++) SetTile(level, x, z, TILE_EMPTY);
    }
}

static void CarveCorridor(Level *level, int x0, int z0, int x1, int z1, bool horizontalFirst)
{
    int stepX = (x1 > x0) ? 1 : -1;
    int stepZ = (z1 > z0) ? 1 : -1;

    if (horizontalFirst)
    {
        for (int x = x0; x != x1 + stepX; x += stepX) SetTile(level, x, z0, TILE_EMPTY);
        for (int z = z0; z != z1 + stepZ; z += stepZ) SetTile(level, x1, z, TILE_EMPTY);
    }
    else
    {
        for (int z = z0; z != z1 + stepZ; z += stepZ) SetTile(level, x0, z, TILE_EMPTY);
        for (int x = x0; x != x1 + stepX; x += stepX) SetTile(level, x, z1, TILE_EMPTY);
    }
}

static bool RoomsOverlap(LevelRoom a, LevelRoom b, int padding)
{
    return (a.x - padding < b.x + b.width) && (a.x + a.width + padding > b.x) &&
           (a.z - padding < b.z + b.depth) && (a.z + a.depth + padding > b.z);
}

static void ConnectRooms(Level *level, LevelRoom a, LevelRoom b)
{
    int ax = a.x + a.width/2;
    int az = a.z + a.depth/2;
    int bx = b.x + b.width/2;
    int bz = b.z + b.depth/2;
    CarveCorridor(level, ax, az, bx, bz, (GetRandomValue(0, 1) == 1));
}

static void PlacePillars(Level *level, LevelRoom room)
{
    // Pillars sit two cells in from each corner so the room stays connected around them
    if ((room.width < 7) || (room.depth < 7)) return;

    int left = room.x + 2;
    int right = room.x + room.width - 3;
    int top = room.z + 2;
    int bottom = room.z + room.depth - 3;

    SetTile(level, left, top, TILE_WALL);
    SetTile(level, right, top, TILE_WALL);
    SetTile(level, left, bottom, TILE_WALL);
    SetTile(level, right, bottom, TILE_WALL);
}

void Level_Generate(Level *level, LevelGenSettings settings)
{
    memset(level, 0, sizeof(Level));

    level->width = (int)Clamp((float)settings.width, 8.0f, (float)LEVEL_MAX_WIDTH);
    level->depth = (int)Clamp((float)settings.depth, 8.0f, (float)LEVEL_MAX_DEPTH);
    level->tileSize = settings.tileSize;
    level->wallHeight = settings.wallHeight;
    level->seed = settings.seed;

    SetRandomSeed(settings.seed);

    // Start fully solid, then carve rooms out of it
    for (int z = 0; z < level->depth; z++)
    {
        for (int x = 0; x < level->width; x++) level->tiles[z][x] = (unsigned char)TILE_WALL;
    }

    int targetRooms = (int)Clamp((float)GetRandomValue(settings.minRooms, settings.maxRooms), 1.0f, (float)LEVEL_MAX_ROOMS);

    for (int attempt = 0; (attempt < 400) && (level->roomCount < targetRooms); attempt++)
    {
        LevelRoom room = { 0 };
        room.width = GetRandomValue(settings.minRoomSize, settings.maxRoomSize);
        room.depth = GetRandomValue(settings.minRoomSize, settings.maxRoomSize);
        if ((room.width >= level->width - 2) || (room.depth >= level->depth - 2)) continue;

        room.x = GetRandomValue(1, level->width - room.width - 1);
        room.z = GetRandomValue(1, level->depth - room.depth - 1);

        bool overlaps = false;
        for (int i = 0; i < level->roomCount; i++)
        {
            if (RoomsOverlap(room, level->rooms[i], 2)) { overlaps = true; break; }
        }
        if (overlaps) continue;

        CarveRoom(level, room);
        if (level->roomCount > 0) ConnectRooms(level, level->rooms[level->roomCount - 1], room);
        level->rooms[level->roomCount++] = room;
    }

    // One extra connection creates a loop so fights are not purely linear
    if (level->roomCount > 3) ConnectRooms(level, level->rooms[0], level->rooms[level->roomCount - 1]);

    for (int i = 0; i < level->roomCount; i++)
    {
        if (GetRandomValue(0, 99) < settings.pillarChance) PlacePillars(level, level->rooms[i]);
    }
}

//----------------------------------------------------------------------------------
// Grid queries
//----------------------------------------------------------------------------------
TileType Level_GetTile(const Level *level, int x, int z)
{
    if ((x < 0) || (z < 0) || (x >= level->width) || (z >= level->depth)) return TILE_WALL;
    return (TileType)level->tiles[z][x];
}

bool Level_IsSolid(const Level *level, int x, int z)
{
    return (Level_GetTile(level, x, z) == TILE_WALL);
}

void Level_WorldToCell(const Level *level, Vector3 position, int *cellX, int *cellZ)
{
    *cellX = (int)floorf(position.x/level->tileSize);
    *cellZ = (int)floorf(position.z/level->tileSize);
}

bool Level_IsSolidAtWorld(const Level *level, Vector3 position)
{
    int x, z;
    Level_WorldToCell(level, position, &x, &z);
    return Level_IsSolid(level, x, z);
}

Vector3 Level_CellToWorld(const Level *level, int cellX, int cellZ)
{
    return (Vector3){ ((float)cellX + 0.5f)*level->tileSize, 0.0f, ((float)cellZ + 0.5f)*level->tileSize };
}

BoundingBox Level_GetCellBounds(const Level *level, int cellX, int cellZ)
{
    float size = level->tileSize;
    return (BoundingBox){
        .min = (Vector3){ (float)cellX*size, 0.0f, (float)cellZ*size },
        .max = (Vector3){ (float)(cellX + 1)*size, level->wallHeight, (float)(cellZ + 1)*size },
    };
}

Vector3 Level_GetRoomCenter(const Level *level, int roomIndex)
{
    LevelRoom room = level->rooms[roomIndex];
    return Level_CellToWorld(level, room.x + room.width/2, room.z + room.depth/2);
}

Vector3 Level_GetCenter(const Level *level)
{
    return (Vector3){ (float)level->width*level->tileSize*0.5f, 0.0f, (float)level->depth*level->tileSize*0.5f };
}

bool Level_FindOpenCellNear(const Level *level, int cellX, int cellZ, int radius, int *outX, int *outZ)
{
    for (int attempt = 0; attempt < 24; attempt++)
    {
        int x = cellX + GetRandomValue(-radius, radius);
        int z = cellZ + GetRandomValue(-radius, radius);
        if (!Level_IsSolid(level, x, z))
        {
            *outX = x;
            *outZ = z;
            return true;
        }
    }
    return false;
}

//----------------------------------------------------------------------------------
// Collision
//----------------------------------------------------------------------------------
bool Level_ResolveCircle(const Level *level, Vector3 *position, float radius, Vector3 *outNormal)
{
    bool collided = false;
    Vector3 totalPush = { 0 };
    float size = level->tileSize;

    // A few passes handle corners where two walls push at once
    for (int pass = 0; pass < 3; pass++)
    {
        bool pushedThisPass = false;
        int cellX, cellZ;
        Level_WorldToCell(level, *position, &cellX, &cellZ);

        for (int z = cellZ - 1; z <= cellZ + 1; z++)
        {
            for (int x = cellX - 1; x <= cellX + 1; x++)
            {
                if (!Level_IsSolid(level, x, z)) continue;

                float minX = (float)x*size, maxX = minX + size;
                float minZ = (float)z*size, maxZ = minZ + size;

                // Closest point on the wall rectangle to the circle center
                float closestX = Clamp(position->x, minX, maxX);
                float closestZ = Clamp(position->z, minZ, maxZ);
                float dx = position->x - closestX;
                float dz = position->z - closestZ;
                float distSq = dx*dx + dz*dz;

                if (distSq >= radius*radius) continue;

                Vector3 push = { 0 };
                if (distSq > 0.000001f)
                {
                    float dist = sqrtf(distSq);
                    float depth = radius - dist;
                    push = (Vector3){ dx/dist*depth, 0.0f, dz/dist*depth };
                }
                else
                {
                    // Center is inside the wall: leave through the closest face
                    float left = position->x - minX, right = maxX - position->x;
                    float top = position->z - minZ, bottom = maxZ - position->z;
                    float best = fminf(fminf(left, right), fminf(top, bottom));
                    if (best == left) push.x = -(left + radius);
                    else if (best == right) push.x = right + radius;
                    else if (best == top) push.z = -(top + radius);
                    else push.z = bottom + radius;
                }

                position->x += push.x;
                position->z += push.z;
                totalPush = Vector3Add(totalPush, push);
                collided = true;
                pushedThisPass = true;
            }
        }

        if (!pushedThisPass) break;
    }

    if (outNormal != NULL) *outNormal = collided ? Vector3Normalize(totalPush) : (Vector3){ 0 };
    return collided;
}

//----------------------------------------------------------------------------------
// Ray queries
//----------------------------------------------------------------------------------
RayCollision Level_Raycast(const Level *level, Ray ray, float maxDistance)
{
    ray.direction = Vector3Normalize(ray.direction);
    Vector3 dir = ray.direction;

    RayCollision result = { 0 };
    result.distance = maxDistance;

    // Floor plane at y = 0
    if (dir.y < -0.0001f)
    {
        float t = -ray.position.y/dir.y;
        if ((t >= 0.0f) && (t < result.distance))
        {
            result.hit = true;
            result.distance = t;
            result.normal = (Vector3){ 0.0f, 1.0f, 0.0f };
        }
    }

    // Walk the grid cells the ray crosses (DDA) and test walls in order
    float size = level->tileSize;
    int cellX, cellZ;
    Level_WorldToCell(level, ray.position, &cellX, &cellZ);

    int stepX = (dir.x > 0.0f) ? 1 : -1;
    int stepZ = (dir.z > 0.0f) ? 1 : -1;
    const float huge = 1.0e30f;

    float tDeltaX = (fabsf(dir.x) > 0.000001f) ? size/fabsf(dir.x) : huge;
    float tDeltaZ = (fabsf(dir.z) > 0.000001f) ? size/fabsf(dir.z) : huge;
    float boundaryX = (float)((stepX > 0) ? cellX + 1 : cellX)*size;
    float boundaryZ = (float)((stepZ > 0) ? cellZ + 1 : cellZ)*size;
    float tMaxX = (fabsf(dir.x) > 0.000001f) ? (boundaryX - ray.position.x)/dir.x : huge;
    float tMaxZ = (fabsf(dir.z) > 0.000001f) ? (boundaryZ - ray.position.z)/dir.z : huge;
    float tCurrent = 0.0f;

    while (tCurrent <= result.distance)
    {
        if ((cellX < 0) || (cellZ < 0) || (cellX >= level->width) || (cellZ >= level->depth)) break;

        if (Level_IsSolid(level, cellX, cellZ))
        {
            RayCollision wallHit = GetRayCollisionBox(ray, Level_GetCellBounds(level, cellX, cellZ));
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
            cellX += stepX;
        }
        else
        {
            tCurrent = tMaxZ;
            tMaxZ += tDeltaZ;
            cellZ += stepZ;
        }
    }

    result.point = Vector3Add(ray.position, Vector3Scale(dir, result.distance));
    return result;
}

bool Level_HasLineOfSight(const Level *level, Vector3 from, Vector3 to)
{
    Vector3 delta = Vector3Subtract(to, from);
    float distance = Vector3Length(delta);
    if (distance < 0.0001f) return true;

    RayCollision hit = Level_Raycast(level, (Ray){ .position = from, .direction = delta }, distance);
    return !hit.hit;
}

//----------------------------------------------------------------------------------
// Rendering
//----------------------------------------------------------------------------------
static Color GetWallColor(int x, int z)
{
    // Cheap per-cell variation so walls do not look like one flat block
    unsigned int hash = ((unsigned int)x*73856093u) ^ ((unsigned int)z*19349663u);
    float variation = 0.9f + (float)(hash % 16u)*0.012f;
    return Render_Shade((Color){ 120, 112, 130, 255 }, variation);
}

static void DrawFloorCell(const Level *level, int x, int z)
{
    float size = level->tileSize;
    Vector3 origin = (Vector3){ (float)x*size, 0.0f, (float)z*size };
    Color base = ((x + z) & 1) ? (Color){ 92, 96, 104, 255 } : (Color){ 76, 80, 88, 255 };
    Vector3 center = Level_CellToWorld(level, x, z);

    Render_DrawQuad3D(origin, (Vector3){ 0.0f, 0.0f, size }, (Vector3){ size, 0.0f, 0.0f }, Render_ApplyFog(base, center));
}

static void DrawWallCell(const Level *level, int x, int z)
{
    float size = level->tileSize;
    float height = level->wallHeight;
    float x0 = (float)x*size, x1 = x0 + size;
    float z0 = (float)z*size, z1 = z0 + size;
    Color base = GetWallColor(x, z);
    Vector3 up = (Vector3){ 0.0f, height, 0.0f };
    Vector3 alongX = (Vector3){ size, 0.0f, 0.0f };
    Vector3 alongZ = (Vector3){ 0.0f, 0.0f, size };

    // Only faces next to open space are visible; each direction gets its own shade for depth
    if (!Level_IsSolid(level, x + 1, z))
    {
        Color c = Render_ApplyFog(Render_Shade(base, 0.85f), (Vector3){ x1, height*0.5f, z0 + size*0.5f });
        Render_DrawQuad3D((Vector3){ x1, 0.0f, z0 }, up, alongZ, c);
    }
    if (!Level_IsSolid(level, x - 1, z))
    {
        Color c = Render_ApplyFog(Render_Shade(base, 0.65f), (Vector3){ x0, height*0.5f, z0 + size*0.5f });
        Render_DrawQuad3D((Vector3){ x0, 0.0f, z0 }, alongZ, up, c);
    }
    if (!Level_IsSolid(level, x, z + 1))
    {
        Color c = Render_ApplyFog(Render_Shade(base, 1.0f), (Vector3){ x0 + size*0.5f, height*0.5f, z1 });
        Render_DrawQuad3D((Vector3){ x0, 0.0f, z1 }, alongX, up, c);
    }
    if (!Level_IsSolid(level, x, z - 1))
    {
        Color c = Render_ApplyFog(Render_Shade(base, 0.75f), (Vector3){ x0 + size*0.5f, height*0.5f, z0 });
        Render_DrawQuad3D((Vector3){ x0, 0.0f, z0 }, up, alongX, c);
    }

    // Top cap (visible while jumping)
    Color top = Render_ApplyFog(Render_Shade(base, 1.15f), (Vector3){ x0 + size*0.5f, height, z0 + size*0.5f });
    Render_DrawQuad3D((Vector3){ x0, height, z0 }, alongZ, alongX, top);
}

static bool IsWallVisible(const Level *level, int x, int z)
{
    return !Level_IsSolid(level, x + 1, z) || !Level_IsSolid(level, x - 1, z) ||
           !Level_IsSolid(level, x, z + 1) || !Level_IsSolid(level, x, z - 1);
}

void Level_Draw(const Level *level)
{
    for (int z = 0; z < level->depth; z++)
    {
        for (int x = 0; x < level->width; x++)
        {
            if (!Level_IsSolid(level, x, z)) DrawFloorCell(level, x, z);
            else if (IsWallVisible(level, x, z)) DrawWallCell(level, x, z);
        }
    }
}
