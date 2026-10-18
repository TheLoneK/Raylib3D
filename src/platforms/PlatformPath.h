#pragma once

#include "raylib.h"

struct PathFollower;

//----------------------------------------------------------------------------------
// Waypoint following for PathFollower (no ECS, so it can be reused or tested alone)
//----------------------------------------------------------------------------------
namespace PlatformPath
{
    // Moves position toward the current stop, pausing at each stop and turning around /
    // wrapping per the path mode. Returns the new position
    Vector3 Advance(PathFollower &path, Vector3 position, float deltaTime);
}
