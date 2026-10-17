#include "ecs/Ecs.h"
#include "ecs/Components.h"
#include "ecs/GameContext.h"

namespace Ecs
{
    void RegisterComponents(flecs::world &world)
    {
        // Registering up front gives every component a readable name in the explorer
        world.component<WorldTransform>();
        world.component<Velocity>();
        world.component<Collider>();
        world.component<Health>();
        world.component<Lifetime>();
        world.component<Dead>();
        world.component<Enemy>();
        world.component<EnemyBrain>();
        world.component<EnemyVisual>();
        world.component<Boss>();
        world.component<DamageZone>();
        world.component<Pickup>();
        world.component<WallSwitch>();
        world.component<Projectile>();
        world.component<Tracer>();
        world.component<Impact>();
        world.component<GameContext>();
    }

    void RegisterCoreSystems(flecs::world &world)
    {
        world.system<Lifetime>("Lifetime")
            .kind(flecs::PostUpdate)
            .each([](flecs::iter &it, size_t row, Lifetime &lifetime) {
                lifetime.remaining -= it.delta_time();
                if (lifetime.remaining <= 0.0f) it.entity(row).destruct();
            });
    }

    void RunSystems(const SystemList &systems)
    {
        for (const flecs::system &system : systems) system.run();
    }

    const GameContext &GetContext(flecs::iter &it)
    {
        flecs::world world = it.world();
        return world.get<GameContext>();
    }

    void EnableExplorer(flecs::world &world)
    {
        world.import<flecs::stats>();
        world.set<flecs::Rest>({});
    }
}
