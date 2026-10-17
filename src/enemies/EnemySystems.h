#pragma once

#include "ecs/Ecs.h"

//----------------------------------------------------------------------------------
// Enemy systems:
//   EnemyAI         senses, state machine, flow field chasing, attacks   (OnUpdate)
//   BuildEnemyGrid  spatial grid of living enemies                       (OnUpdate)
//   EnemySeparation keeps the crowd from overlapping                     (OnUpdate)
//   EnemyVisuals    hit flash fade                                       (OnUpdate)
//   DrawEnemies / DrawCorpses                                            (draw list)
//----------------------------------------------------------------------------------
namespace EnemySystems
{
    void Register(flecs::world &world, SystemList &drawSystems);
}
