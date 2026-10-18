#include "platforms/Platforms.h"
#include "core/Render.h"
#include "platforms/PlatformPath.h"
#include "player/Player.h"
#include "world/SolidBody.h"
#include "raymath.h"
#include <algorithm>

namespace
{
    // Each face gets its own shade, like the level walls
    void DrawPlatformBox(const BoundingBox &box, Color base)
    {
        Vector3 center = (box.min + box.max)*0.5f;
        if (Render::IsFullyFogged(center, Vector3Distance(box.min, box.max)*0.5f)) return;

        Vector3 size = box.max - box.min;
        Vector3 alongX = { size.x, 0.0f, 0.0f };
        Vector3 alongY = { 0.0f, size.y, 0.0f };
        Vector3 alongZ = { 0.0f, 0.0f, size.z };
        Vector3 min = box.min, max = box.max;
        auto shade = [&](float factor) { return Render::ApplyFog(Render::Shade(base, factor), center); };

        Render::DrawQuad3D(Vector3{ min.x, max.y, min.z }, alongZ, alongX, shade(1.2f));    // Top
        Render::DrawQuad3D(min, alongX, alongZ, shade(0.45f));                                // Bottom
        Render::DrawQuad3D(Vector3{ max.x, min.y, min.z }, alongY, alongZ, shade(0.85f));    // +X
        Render::DrawQuad3D(min, alongZ, alongY, shade(0.65f));                                // -X
        Render::DrawQuad3D(Vector3{ min.x, min.y, max.z }, alongX, alongY, shade(1.0f));     // +Z
        Render::DrawQuad3D(min, alongY, alongX, shade(0.75f));                                // -Z

        // Dark rim around the top so the edge reads clearly when jumping on
        BoundingBox rim = { Vector3{ min.x, max.y - 0.08f, min.z }, Vector3{ max.x, max.y + 0.005f, max.z } };
        DrawBoundingBox(rim, shade(0.35f));
    }
}

namespace Platforms
{
    flecs::entity Spawn(flecs::world &world, const PlatformSpawn &spawn)
    {
        PathFollower path = {};
        path.count = std::min(static_cast<int>(spawn.points.size()), MaxPathPoints);
        std::copy_n(spawn.points.begin(), path.count, path.points);
        path.mode = spawn.mode;
        path.speed = spawn.speed;
        path.target = (path.count > 1) ? 1 : 0;
        path.step = 1;
        path.waitTimer = (path.count > 0) ? path.points[0].wait : 0.0f;

        Vector3 start = (path.count > 0) ? path.points[0].position : Vector3{ 0.0f, 0.0f, 0.0f };
        return world.entity()
            .set(WorldTransform{ start, 0.0f })
            .set(Velocity{ Vector3{ 0.0f, 0.0f, 0.0f } })
            .set(SolidBox{ spawn.size })
            .set(path)
            .set(Platform{ spawn.color });
    }

    void RegisterSystems(flecs::world &world, SystemList &drawSystems)
    {
        // Runs after the player moved this frame: whoever stands on a platform is carried with it
        world.system<WorldTransform, Velocity, PathFollower, const SolidBox>("MovePlatforms")
            .each([](flecs::iter &it, size_t row, WorldTransform &transform, Velocity &velocity, PathFollower &path, const SolidBox &box) {
                float deltaTime = it.delta_time();
                Vector3 previous = transform.position;
                transform.position = PlatformPath::Advance(path, previous, deltaTime);

                Vector3 delta = transform.position - previous;
                velocity.value = (deltaTime > 0.0f) ? delta/deltaTime : Vector3{ 0.0f, 0.0f, 0.0f };

                SolidBody body = { it.entity(row).id(), SolidBodies::BoxFromTop(transform.position, box.size), velocity.value };
                Ecs::GetContext(it).player->OnBodyMoved(body, delta);
            });

        // Velocity is optional so static solid boxes (crates, ledges) work too
        world.system<const WorldTransform, const SolidBox, const Velocity *>("CollectSolidBodies")
            .kind(flecs::PostUpdate)
            .run([](flecs::iter &it) {
                SolidBodyList &bodies = *Ecs::GetContext(it).solidBodies;
                bodies.clear();
                while (it.next())
                {
                    auto transforms = it.field<const WorldTransform>(0);
                    auto boxes = it.field<const SolidBox>(1);
                    bool moving = it.is_set(2);
                    for (auto row : it)
                    {
                        Vector3 velocity = moving ? it.field<const Velocity>(2)[row].value : Vector3{ 0.0f, 0.0f, 0.0f };
                        bodies.push_back(SolidBody{ it.entity(row).id(), SolidBodies::BoxFromTop(transforms[row].position, boxes[row].size), velocity });
                    }
                }
            });

        drawSystems.push_back(world.system<const WorldTransform, const SolidBox, const Platform>("DrawPlatforms")
            .kind(0)
            .each([](const WorldTransform &transform, const SolidBox &box, const Platform &platform) {
                DrawPlatformBox(SolidBodies::BoxFromTop(transform.position, box.size), platform.color);
            }));
    }
}
