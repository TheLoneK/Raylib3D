#include "combat/Effects.h"
#include "core/RenderAssets.h"
#include "ecs/GameContext.h"

namespace
{
    constexpr float TracerTime = 0.06f;
    constexpr float ImpactTime = 0.25f;
}

namespace Effects
{
    void SpawnTracer(flecs::world &world, Vector3 start, Vector3 end, Color color)
    {
        world.entity()
            .set(Tracer{ start, end, color })
            .set(Lifetime{ TracerTime, TracerTime });
    }

    void SpawnImpact(flecs::world &world, Vector3 position, float size, Color color)
    {
        world.entity()
            .set(Impact{ position, size, color })
            .set(Lifetime{ ImpactTime, ImpactTime });
    }

    void RegisterSystems(flecs::world &world, SystemList &drawSystems)
    {
        drawSystems.push_back(world.system<const Tracer, const Lifetime>("DrawTracers")
            .kind(0)
            .each([](const Tracer &tracer, const Lifetime &lifetime) {
                DrawLine3D(tracer.start, tracer.end, Fade(tracer.color, lifetime.remaining/lifetime.total));
            }));

        drawSystems.push_back(world.system<const Impact, const Lifetime>("DrawImpacts")
            .kind(0)
            .each([](flecs::iter &it, size_t, const Impact &impact, const Lifetime &lifetime) {
                float t = lifetime.remaining/lifetime.total;   // 1 -> 0
                float size = impact.size*(1.0f + (1.0f - t)*1.5f);
                Ecs::GetContext(it).assets->DrawSphere(impact.position, size, Fade(impact.color, t));
            }));
    }
}
