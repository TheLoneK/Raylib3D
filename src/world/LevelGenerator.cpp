#include "world/LevelGenerator.h"
#include "core/MathUtils.h"
#include "world/LinkTags.h"
#include "raymath.h"
#include <algorithm>
#include <iterator>
#include <queue>

//----------------------------------------------------------------------------------
// Presets
//----------------------------------------------------------------------------------
LevelGenSettings LevelGenSettings::Small(unsigned int seed)
{
    LevelGenSettings settings;
    settings.seed = seed;
    return settings;
}

LevelGenSettings LevelGenSettings::Large(unsigned int seed)
{
    LevelGenSettings settings;
    settings.seed = seed;
    settings.width = 70;            // 70x70 cells = ~3x the area of the 40x40 small level
    settings.depth = 70;
    settings.minRooms = 20;
    settings.maxRooms = 28;
    settings.minRoomSize = 5;
    settings.maxRoomSize = 11;
    settings.loopChance = 35;
    settings.lockedDoors = true;
    settings.bossRoom = true;
    settings.ammoChance = 60;
    settings.healthChance = 35;
    settings.damageBoostCount = 3;
    return settings;
}

namespace
{
    struct Lock
    {
        int edge;           // Chain edge: connects room edge -> room edge + 1
        int link;
        DoorLock kind;
        CellCoord cell;
        bool placed;
    };

    // Holds the in-progress state while one level is generated
    class Builder
    {
    public:
        Builder(Level &level, const LevelGenSettings &settings) : m_level(level), m_settings(settings) {}

        LevelFeatures Run();
        bool IsBossGated() const;       // The last door (in front of the boss) survived placement

    private:
        void PlaceRooms();
        void OrderRoomsAsChain();
        std::vector<CellCoord> CarveCorridor(CellCoord from, CellCoord to);
        void CarveConnections();
        void PlacePillars(const LevelRoom &room);
        void ChooseLocks();
        int GetSegment(int roomIndex) const;
        bool IsInsideAnyRoom(CellCoord cell) const;
        bool FindDoorCell(int edge, CellCoord &outCell, bool &outSpansX) const;
        void PlaceDoors();
        std::vector<bool> FloodFill(CellCoord start, const std::vector<CellCoord> &blocked) const;
        void RemoveBypassedDoors();
        CellCoord RandomCellInRoom(const LevelRoom &room) const;
        int PickRoomInRange(int first, int last) const;
        void PlaceKeysAndSwitches(LevelFeatures &features) const;
        void PlacePickups(LevelFeatures &features) const;

        Level &m_level;
        const LevelGenSettings &m_settings;
        std::vector<LevelRoom> m_rooms;                     // In chain order: room i connects to room i + 1
        std::vector<std::vector<CellCoord>> m_chainPaths;   // Corridor cells for each chain edge
        std::vector<Lock> m_locks;                          // Ordered by edge
    };

    bool RoomsOverlap(const LevelRoom &a, const LevelRoom &b, int padding)
    {
        return (a.x - padding < b.x + b.width) && (a.x + a.width + padding > b.x) &&
               (a.z - padding < b.z + b.depth) && (a.z + a.depth + padding > b.z);
    }

    float CenterDistance(const LevelRoom &a, const LevelRoom &b)
    {
        CellCoord ca = a.Center(), cb = b.Center();
        return Vector2Distance(Vector2{ static_cast<float>(ca.x), static_cast<float>(ca.z) }, Vector2{ static_cast<float>(cb.x), static_cast<float>(cb.z) });
    }

    //------------------------------------------------------------------------------
    // Rooms and corridors
    //------------------------------------------------------------------------------
    void Builder::PlaceRooms()
    {
        int targetRooms = GetRandomValue(m_settings.minRooms, m_settings.maxRooms);
        int width = m_level.GetWidth(), depth = m_level.GetDepth();

        for (int attempt = 0; (attempt < targetRooms*60) && (static_cast<int>(m_rooms.size()) < targetRooms); attempt++)
        {
            LevelRoom room = {};
            room.width = GetRandomValue(m_settings.minRoomSize, m_settings.maxRoomSize);
            room.depth = GetRandomValue(m_settings.minRoomSize, m_settings.maxRoomSize);
            if ((room.width >= width - 2) || (room.depth >= depth - 2)) continue;

            room.x = GetRandomValue(1, width - room.width - 1);
            room.z = GetRandomValue(1, depth - room.depth - 1);

            bool overlaps = std::any_of(m_rooms.begin(), m_rooms.end(), [&](const LevelRoom &other) { return RoomsOverlap(room, other, 2); });
            if (!overlaps) m_rooms.push_back(room);
        }

        // Always have somewhere to stand
        if (m_rooms.empty()) m_rooms.push_back(LevelRoom{ width/2 - 3, depth/2 - 3, 6, 6 });

        for (const LevelRoom &room : m_rooms)
        {
            for (int z = room.z; z < room.z + room.depth; z++)
            {
                for (int x = room.x; x < room.x + room.width; x++) m_level.SetTile(x, z, TileType::Empty);
            }
        }
    }

    // Greedy nearest-neighbor chain from the room nearest a corner, so corridors stay short
    // and progression sweeps across the map
    void Builder::OrderRoomsAsChain()
    {
        std::vector<LevelRoom> remaining = m_rooms;
        m_rooms.clear();

        auto first = std::min_element(remaining.begin(), remaining.end(), [](const LevelRoom &a, const LevelRoom &b) {
            return (a.Center().x + a.Center().z) < (b.Center().x + b.Center().z);
        });
        m_rooms.push_back(*first);
        remaining.erase(first);

        while (!remaining.empty())
        {
            const LevelRoom &last = m_rooms.back();
            auto next = std::min_element(remaining.begin(), remaining.end(), [&](const LevelRoom &a, const LevelRoom &b) {
                return CenterDistance(last, a) < CenterDistance(last, b);
            });
            m_rooms.push_back(*next);
            remaining.erase(next);
        }
    }

    std::vector<CellCoord> Builder::CarveCorridor(CellCoord from, CellCoord to)
    {
        std::vector<CellCoord> path;
        bool horizontalFirst = (GetRandomValue(0, 1) == 1);
        int stepX = (to.x > from.x) ? 1 : -1;
        int stepZ = (to.z > from.z) ? 1 : -1;
        CellCoord cell = from;

        auto visit = [&](CellCoord c) {
            if (m_level.IsWall(c.x, c.z)) m_level.SetTile(c.x, c.z, TileType::Empty);
            path.push_back(c);
        };

        visit(cell);
        if (horizontalFirst)
        {
            while (cell.x != to.x) { cell.x += stepX; visit(cell); }
            while (cell.z != to.z) { cell.z += stepZ; visit(cell); }
        }
        else
        {
            while (cell.z != to.z) { cell.z += stepZ; visit(cell); }
            while (cell.x != to.x) { cell.x += stepX; visit(cell); }
        }
        return path;
    }

    int Builder::GetSegment(int roomIndex) const
    {
        // Rooms past a locked edge belong to the next segment
        int segment = 0;
        for (const Lock &lock : m_locks)
        {
            if (lock.edge < roomIndex) segment++;
        }
        return segment;
    }

    void Builder::CarveConnections()
    {
        for (size_t i = 0; i + 1 < m_rooms.size(); i++) m_chainPaths.push_back(CarveCorridor(m_rooms[i].Center(), m_rooms[i + 1].Center()));

        int roomCount = static_cast<int>(m_rooms.size());
        if (m_locks.empty() && (roomCount > 3))
        {
            // One loop back to the start so fights are not purely linear
            CarveCorridor(m_rooms.front().Center(), m_rooms.back().Center());
        }

        // Extra loops, but never across a lock (that would let the player skip the door)
        for (int i = 0; i + 2 < roomCount; i++)
        {
            if ((GetSegment(i) == GetSegment(i + 2)) && (GetRandomValue(0, 99) < m_settings.loopChance))
            {
                CarveCorridor(m_rooms[static_cast<size_t>(i)].Center(), m_rooms[static_cast<size_t>(i + 2)].Center());
            }
        }
    }

    void Builder::PlacePillars(const LevelRoom &room)
    {
        // Pillars sit two cells in from each corner so the room stays connected around them
        if ((room.width < 7) || (room.depth < 7)) return;

        int left = room.x + 2, right = room.x + room.width - 3;
        int top = room.z + 2, bottom = room.z + room.depth - 3;
        for (CellCoord cell : { CellCoord{ left, top }, CellCoord{ right, top }, CellCoord{ left, bottom }, CellCoord{ right, bottom } })
        {
            m_level.SetTile(cell.x, cell.z, TileType::Wall);
        }
    }

    //------------------------------------------------------------------------------
    // Locks and doors
    //------------------------------------------------------------------------------
    void Builder::ChooseLocks()
    {
        int edgeCount = static_cast<int>(m_rooms.size()) - 1;
        if (!m_settings.lockedDoors || (edgeCount < 2)) return;

        std::vector<int> edges;
        int edge = GetRandomValue(1, 2);
        int maxLocks = m_settings.bossRoom ? LinkTags::Count - 1 : LinkTags::Count;
        while ((edge < edgeCount - 1) && (static_cast<int>(edges.size()) < maxLocks))
        {
            edges.push_back(edge);
            edge += GetRandomValue(m_settings.minRoomsPerLock, m_settings.maxRoomsPerLock);
        }
        if (m_settings.bossRoom) edges.push_back(edgeCount - 1);   // The boss is always behind a door

        // Mix both lock kinds
        bool keyFirst = (GetRandomValue(0, 1) == 0);
        for (size_t i = 0; i < edges.size(); i++)
        {
            bool isKey = ((i%2 == 0) == keyFirst);
            m_locks.push_back(Lock{ edges[i], static_cast<int>(i), isKey ? DoorLock::Key : DoorLock::Switch, CellCoord{ -1, -1 }, false });
        }
    }

    bool Builder::IsInsideAnyRoom(CellCoord cell) const
    {
        return std::any_of(m_rooms.begin(), m_rooms.end(), [&](const LevelRoom &room) { return room.Contains(cell); });
    }

    // A door needs a corridor cell with walls on exactly two opposite sides, so it seals the gap
    bool Builder::FindDoorCell(int edge, CellCoord &outCell, bool &outSpansX) const
    {
        const std::vector<CellCoord> &path = m_chainPaths[static_cast<size_t>(edge)];

        // Search from the locked room's end, so the door sits at its entrance
        for (auto it = path.rbegin(); it != path.rend(); ++it)
        {
            CellCoord cell = *it;
            if (m_level.IsWall(cell.x, cell.z) || IsInsideAnyRoom(cell) || (m_level.FindDoor(cell) != nullptr)) continue;

            bool wallsOnX = m_level.IsWall(cell.x - 1, cell.z) && m_level.IsWall(cell.x + 1, cell.z);
            bool wallsOnZ = m_level.IsWall(cell.x, cell.z - 1) && m_level.IsWall(cell.x, cell.z + 1);
            if (wallsOnX == wallsOnZ) continue;     // Open junction (or fully enclosed)

            outCell = cell;
            outSpansX = wallsOnX;
            return true;
        }
        return false;
    }

    void Builder::PlaceDoors()
    {
        for (Lock &lock : m_locks)
        {
            bool spansX = false;
            if (!FindDoorCell(lock.edge, lock.cell, spansX)) continue;

            m_level.AddDoor(lock.cell, spansX, lock.link, lock.kind);
            lock.placed = true;
        }
    }

    std::vector<bool> Builder::FloodFill(CellCoord start, const std::vector<CellCoord> &blocked) const
    {
        int width = m_level.GetWidth();
        std::vector<bool> reached(static_cast<size_t>(width*m_level.GetDepth()), false);
        auto isBlocked = [&](CellCoord cell) {
            return m_level.IsWall(cell.x, cell.z) || (std::find(blocked.begin(), blocked.end(), cell) != blocked.end());
        };

        std::queue<CellCoord> open;
        open.push(start);
        reached[static_cast<size_t>(start.z*width + start.x)] = true;

        while (!open.empty())
        {
            CellCoord cell = open.front();
            open.pop();
            for (CellCoord step : { CellCoord{ 1, 0 }, CellCoord{ -1, 0 }, CellCoord{ 0, 1 }, CellCoord{ 0, -1 } })
            {
                CellCoord next = { cell.x + step.x, cell.z + step.z };
                size_t index = static_cast<size_t>(next.z*width + next.x);
                if (!m_level.IsInside(next.x, next.z) || reached[index] || isBlocked(next)) continue;
                reached[index] = true;
                open.push(next);
            }
        }
        return reached;
    }

    // Corridors can cross each other, so a door might not actually gate anything.
    // Check each lock in order (earlier doors open, this and later ones closed) and drop doors that can be walked around
    void Builder::RemoveBypassedDoors()
    {
        int width = m_level.GetWidth();
        CellCoord start = m_rooms.front().Center();

        for (size_t k = 0; k < m_locks.size(); k++)
        {
            if (!m_locks[k].placed) continue;

            std::vector<CellCoord> closed;
            for (size_t j = k; j < m_locks.size(); j++)
            {
                if (m_locks[j].placed) closed.push_back(m_locks[j].cell);
            }

            CellCoord behind = m_rooms[static_cast<size_t>(m_locks[k].edge + 1)].Center();
            std::vector<bool> reached = FloodFill(start, closed);
            if (reached[static_cast<size_t>(behind.z*width + behind.x)])
            {
                m_level.RemoveDoor(m_locks[k].cell);
                m_locks[k].placed = false;
            }
        }
    }

    //------------------------------------------------------------------------------
    // Spawn points
    //------------------------------------------------------------------------------
    CellCoord Builder::RandomCellInRoom(const LevelRoom &room) const
    {
        for (int attempt = 0; attempt < 20; attempt++)
        {
            CellCoord cell = { GetRandomValue(room.x, room.x + room.width - 1), GetRandomValue(room.z, room.z + room.depth - 1) };
            if (!m_level.IsWall(cell.x, cell.z)) return cell;
        }
        return room.Center();
    }

    int Builder::PickRoomInRange(int first, int last) const
    {
        // Avoid the start room when there is a choice
        if ((first == 0) && (last > 0)) first = 1;
        return GetRandomValue(first, std::max(first, last));
    }

    void Builder::PlaceKeysAndSwitches(LevelFeatures &features) const
    {
        float tileSize = m_level.GetTileSize();
        int segmentStart = 0;

        for (const Lock &lock : m_locks)
        {
            if (!lock.placed) continue;

            // Anywhere between the previous door and this one is reachable once the earlier doors are open
            const LevelRoom &room = m_rooms[static_cast<size_t>(PickRoomInRange(segmentStart, lock.edge))];
            segmentStart = lock.edge + 1;

            if (lock.kind == DoorLock::Key)
            {
                features.keys.push_back(KeySpawn{ m_level.CellToWorld(RandomCellInRoom(room)), lock.link });
                continue;
            }

            // Switch: any wall face along the inside of the room
            std::vector<SwitchSpawn> faces;
            for (int z = room.z; z < room.z + room.depth; z++)
            {
                for (int x = room.x; x < room.x + room.width; x++)
                {
                    if (m_level.IsWall(x, z)) continue;
                    for (CellCoord step : { CellCoord{ 1, 0 }, CellCoord{ -1, 0 }, CellCoord{ 0, 1 }, CellCoord{ 0, -1 } })
                    {
                        if (!m_level.IsWall(x + step.x, z + step.z)) continue;
                        Vector3 direction = Vector3{ static_cast<float>(step.x), 0.0f, static_cast<float>(step.z) };
                        Vector3 position = m_level.CellToWorld(CellCoord{ x, z }) + direction*(tileSize*0.5f - 0.06f) + Vector3{ 0.0f, 1.4f, 0.0f };
                        faces.push_back(SwitchSpawn{ position, direction*-1.0f, lock.link });
                    }
                }
            }
            if (!faces.empty()) features.switches.push_back(faces[static_cast<size_t>(GetRandomValue(0, static_cast<int>(faces.size()) - 1))]);
        }
    }

    void Builder::PlacePickups(LevelFeatures &features) const
    {
        constexpr WeaponType ammoTypes[] = { WeaponType::Pistol, WeaponType::Shotgun, WeaponType::Rifle, WeaponType::Plasma };
        int roomCount = static_cast<int>(m_rooms.size());

        for (int i = 1; i < roomCount; i++)
        {
            const LevelRoom &room = m_rooms[static_cast<size_t>(i)];
            if (GetRandomValue(0, 99) < m_settings.ammoChance)
            {
                WeaponType weapon = ammoTypes[GetRandomValue(0, static_cast<int>(std::size(ammoTypes)) - 1)];
                features.pickups.push_back(PickupSpawn{ m_level.CellToWorld(RandomCellInRoom(room)), PickupType::Ammo, weapon });
            }
            if (GetRandomValue(0, 99) < m_settings.healthChance)
            {
                features.pickups.push_back(PickupSpawn{ m_level.CellToWorld(RandomCellInRoom(room)), PickupType::Health, WeaponType::None });
            }
        }

        // Damage boosts favor the later part of the level
        for (int i = 0; (i < m_settings.damageBoostCount) && (roomCount > 1); i++)
        {
            const LevelRoom &room = m_rooms[static_cast<size_t>(GetRandomValue(std::max(1, roomCount/3), roomCount - 1))];
            features.pickups.push_back(PickupSpawn{ m_level.CellToWorld(RandomCellInRoom(room)), PickupType::DamageBoost, WeaponType::None });
        }
    }

    bool Builder::IsBossGated() const
    {
        return !m_locks.empty() && m_locks.back().placed && (m_locks.back().edge == static_cast<int>(m_rooms.size()) - 2);
    }

    LevelFeatures Builder::Run()
    {
        PlaceRooms();
        OrderRoomsAsChain();
        for (const LevelRoom &room : m_rooms) m_level.AddRoom(room);

        ChooseLocks();
        CarveConnections();
        for (const LevelRoom &room : m_rooms)
        {
            if (GetRandomValue(0, 99) < m_settings.pillarChance) PlacePillars(room);
        }
        PlaceDoors();
        RemoveBypassedDoors();

        LevelFeatures features;
        features.playerStart = m_level.GetRoomCenter(0);
        if (m_rooms.size() > 1) features.playerYaw = MathUtils::YawFromDirection(MathUtils::FlatDirection(features.playerStart, m_level.GetRoomCenter(1)));

        PlaceKeysAndSwitches(features);
        PlacePickups(features);

        if (m_settings.bossRoom && (m_rooms.size() >= 3))
        {
            features.hasBoss = true;
            features.bossPosition = m_level.GetRoomCenter(static_cast<int>(m_rooms.size()) - 1);
        }
        return features;
    }
}

namespace LevelGenerator
{
    LevelFeatures Generate(Level &level, const LevelGenSettings &settings)
    {
        // A few layouts have no clean chokepoint in front of the boss room; retry those
        // with a derived seed so the same seed always gives the same level
        constexpr int MaxAttempts = 8;
        LevelFeatures features;

        for (int attempt = 0; attempt < MaxAttempts; attempt++)
        {
            level.Reset(settings.width, settings.depth, settings.tileSize, settings.wallHeight, settings.seed);
            SetRandomSeed(settings.seed + static_cast<unsigned int>(attempt)*104729u);

            Builder builder(level, settings);
            features = builder.Run();
            if (!settings.bossRoom || !settings.lockedDoors || builder.IsBossGated()) break;
        }
        return features;
    }
}
