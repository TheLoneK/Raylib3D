#include "platforms/PlatformTypes.h"

namespace
{
    constexpr float ElevatorLowTop = 0.15f;         // Below SolidBodies::StepHeight, so it is walked onto
    constexpr float ElevatorBuried = 0.4f;          // Column still reaches below the floor at the top stop
    constexpr Color ElevatorColor = { 200, 150, 60, 255 };
    constexpr Color ShuttleColor = { 70, 170, 190, 255 };
}

PlatformSpawn PlatformSpawn::Elevator(Vector3 floorCenter, float width, float topHeight, float speed, float wait)
{
    PlatformSpawn spawn;
    spawn.size = Vector3{ width, topHeight + ElevatorBuried, width };
    spawn.points = {
        PathPoint{ Vector3{ floorCenter.x, ElevatorLowTop, floorCenter.z }, wait },
        PathPoint{ Vector3{ floorCenter.x, topHeight, floorCenter.z }, wait },
    };
    spawn.mode = PathMode::PingPong;
    spawn.speed = speed;
    spawn.color = ElevatorColor;
    return spawn;
}

PlatformSpawn PlatformSpawn::Shuttle(const std::vector<Vector3> &stops, Vector3 size, PathMode mode, float speed, float wait)
{
    PlatformSpawn spawn;
    spawn.size = size;
    for (Vector3 stop : stops) spawn.points.push_back(PathPoint{ stop, wait });
    spawn.mode = mode;
    spawn.speed = speed;
    spawn.color = ShuttleColor;
    return spawn;
}
