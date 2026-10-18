#pragma once

#include "raylib.h"
#include <vector>

//----------------------------------------------------------------------------------
// Moving platform data shared by the level generator (which places them) and the
// platform systems (which move them). No ECS here.
//----------------------------------------------------------------------------------
enum class PathMode
{
    Loop,           // Last stop -> first stop -> ...
    PingPong        // Walks the stops forward, then back
};

struct PathPoint
{
    Vector3 position;       // Center of the platform's top face at this stop
    float wait;             // Seconds to pause on arrival
};

inline constexpr int MaxPathPoints = 8;

struct PlatformSpawn
{
    Vector3 size;                       // Full box size; the box hangs below the path points
    std::vector<PathPoint> points;      // Starts at the first one (up to MaxPathPoints)
    PathMode mode = PathMode::PingPong;
    float speed = 3.0f;                 // Units per second
    Color color = { 200, 150, 60, 255 };

    // A column rising out of the floor: steps on at floor level, lifts the rider up to topHeight
    static PlatformSpawn Elevator(Vector3 floorCenter, float width, float topHeight, float speed = 2.5f, float wait = 2.0f);

    // A slab carrying riders through the stops (positions are top-center)
    static PlatformSpawn Shuttle(const std::vector<Vector3> &stops, Vector3 size, PathMode mode, float speed = 3.0f, float wait = 1.0f);
};
