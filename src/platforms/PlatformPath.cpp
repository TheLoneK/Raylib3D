#include "platforms/PlatformPath.h"
#include "ecs/Components.h"
#include "raymath.h"
#include <algorithm>

namespace
{
    int NextStop(PathFollower &path)
    {
        if (path.mode == PathMode::Loop) return (path.target + 1)%path.count;

        // PingPong: turn around at either end
        int next = path.target + path.step;
        if ((next < 0) || (next >= path.count))
        {
            path.step = -path.step;
            next = path.target + path.step;
        }
        return next;
    }
}

namespace PlatformPath
{
    Vector3 Advance(PathFollower &path, Vector3 position, float deltaTime)
    {
        if ((path.count < 2) || (path.speed <= 0.0f)) return position;

        // Spend the frame's time on waiting and moving, so a stop passed mid-frame doesn't lose time.
        // Each pass reaches a stop or uses up the time; the cap stops paths with only zero-length legs
        float time = deltaTime;
        for (int pass = 0; (pass < path.count*2) && (time > 0.0f); pass++)
        {
            if (path.waitTimer > 0.0f)
            {
                float waited = std::min(path.waitTimer, time);
                path.waitTimer -= waited;
                time -= waited;
                continue;
            }

            const PathPoint &stop = path.points[path.target];
            Vector3 toStop = stop.position - position;
            float distance = Vector3Length(toStop);
            float reach = path.speed*time;
            if (reach < distance) return position + toStop*(reach/distance);

            position = stop.position;
            time -= distance/path.speed;
            path.waitTimer = stop.wait;
            path.target = NextStop(path);
        }
        return position;
    }
}
