#pragma once

#include "flecs.h"
#include "ecs/GameContext.h"
#include <vector>

//----------------------------------------------------------------------------------
// ECS setup shared by every feature.
//
// Update systems are registered with a phase (flecs::PreUpdate / OnUpdate / PostUpdate)
// and run automatically by world.progress(). Draw systems must run between
// BeginMode3D/EndMode3D, so they are registered without a phase and collected in a
// SystemList that the game runs manually while drawing.
//----------------------------------------------------------------------------------
using SystemList = std::vector<flecs::system>;

namespace Ecs
{
    void RegisterComponents(flecs::world &world);
    void RegisterCoreSystems(flecs::world &world);          // Lifetime countdown
    void RunSystems(const SystemList &systems);
    const GameContext &GetContext(flecs::iter &it);      // Shared game state from inside a system
    void EnableExplorer(flecs::world &world);               // Flecs explorer / REST API on localhost:27750
}
