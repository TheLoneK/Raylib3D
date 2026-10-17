# Raylib3D
simple setup of a 3d raylib game for me to play around with

A first person shooter framework in C++20 + raylib, using [flecs](https://www.flecs.dev) as an entity component system (ECS). It has a generated level with collisions, a crowd of enemies that path toward the player with a shared flow field, hitscan and projectile weapons, a 7 slot inventory with per-weapon ammo/reloading, a minimap and a HUD. There is no win condition yet.

## Building

Requirements: CMake 3.16+ and a MinGW gcc/g++ that supports C++20 (GCC 10 or newer).

```powershell
./run.ps1            # configure (first time), build what changed, run
./run.ps1 -NoRun     # build only
./run.ps1 -Debug     # debug build in build/Debug
./run.ps1 -Clean     # wipe build/ and start over
```

If PowerShell blocks the script, see `UsefulScripts.md`.

- **raylib**: if `lib/` contains your prebuilt `libraylib.a` (headers in `include/`) it is used. Otherwise CMake downloads and builds raylib 6.2 automatically (needs git, first build only).
- **flecs** lives in `third_party/flecs/` and is compiled once as a static library. The first build takes a minute because of it; after that only changed files rebuild.
- Every `.cpp` under `src/` is picked up automatically, so new files and folders need no build changes.
- The game is built as `TestGame.exe` in the project root.

## Controls

| Key | Action |
| --- | --- |
| W A S D | Move |
| Space / Left Ctrl | Jump / crouch |
| Mouse | Look |
| Left mouse | Fire (hold for automatic weapons) |
| R | Reload |
| 1 - 7, mouse wheel | Select weapon slot |
| Q | Previous weapon |
| M | Toggle expanded map |
| F1 | Toggle help |
| F2 | God mode (debug) |
| F3 | Spawn another wave of enemies |
| F5 | Generate a new level |
| Enter | Restart the level after dying |

## Code layout

```
src/
  main.cpp          window + main loop only
  core/             Config.h (tuning), MathUtils, Render (fog/quads), RenderAssets (shared GPU meshes)
  ecs/              Components.h, GameContext.h, Ecs (component registration, Lifetime system)
  world/            Level (generation, collision, raycasts), FlowField (pathing), SpatialGrid (neighbor queries)
  player/           Player (movement, camera, health), PlayerInput
  weapons/          Weapon (definition table + ammo/reload state), Inventory (7 slots, switching)
  enemies/          EnemyDefs (stats table), Enemies (spawn/damage/alerts), EnemySystems (AI, crowd, drawing)
  combat/           PlayerCombat (shooting), Projectiles, Effects
  ui/               Hud, Minimap
  game/             Game: owns everything and runs a frame
third_party/flecs/  flecs.c / flecs.h
```

## How the ECS is used

Things that exist in large numbers are **entities**; their data lives in plain **components** (`src/ecs/Components.h`) and the logic lives in **systems**:

| Entity | Components |
| --- | --- |
| Enemy | `WorldTransform`, `Collider`, `Health`, `Enemy`, `EnemyBrain`, `EnemyVisual` |
| Corpse | an enemy that also has `Dead` + `Lifetime` |
| Projectile | `WorldTransform`, `Velocity`, `Projectile`, `Lifetime` |
| Tracer / impact | `Tracer` or `Impact`, plus `Lifetime` |

Each frame `Game::Update` moves the player, updates the flow field, handles shooting, then calls `world.progress()`, which runs the update systems in order:

1. `EnemyAI`: sight checks (staggered, 5 per second per enemy), the idle / wander / chase / attack state machine, and following the flow field
2. `BuildEnemyGrid`: puts living enemies into the spatial grid
3. `EnemySeparation`: pushes overlapping enemies apart (and out of the player)
4. `EnemyVisuals`, `MoveProjectiles`, then `Lifetime` (destroys expired entities)

Draw systems (`DrawEnemies`, `DrawCorpses`, `DrawProjectiles`, `DrawTracers`, `DrawImpacts`) are registered without a phase and run by `Game::Draw` inside `BeginMode3D`/`EndMode3D`.

Systems get the non-ECS state (level, player, flow field...) through the `GameContext` singleton: `Ecs::GetContext(it)`.

**Pathfinding**: `FlowField` runs one shortest-path search outward from the player's cell whenever the player changes cell. Each enemy then just looks up "which neighbor cell next?" for its own cell, so 200 enemies cost the same as one search. When an enemy can see the player up close it walks straight at them instead.

## Extending

- **New weapon**: add a value to `WeaponType` (`weapons/Weapon.h`) and its entry in `s_weaponDefs` (`weapons/Weapon.cpp`). Give it with `Inventory::GiveWeapon` / `SetSlot` (see `Game::GiveStartingLoadout`).
- **New enemy**: add a value to `EnemyType` (`enemies/EnemyDefs.h`) and its entry in `s_enemyDefs`. Melee vs ranged comes from `attackKind`. Spawn it with `Enemies::Spawn`.
- **New entity kind** (pickups, barrels...): add components to `ecs/Components.h`, register them in `Ecs::RegisterComponents`, and write a `RegisterSystems(world, drawSystems)` function in a feature folder; call it from the `Game` constructor.
- **Enemy count / behavior**: `core/Config.h` (`EnemySpawnCount`, `GunnerFraction`, `EnemiesHuntOnSpawn`).
- **Debugging the ECS**: set `EnableFlecsExplorer = true` in `core/Config.h`, run the game and open https://www.flecs.dev/explorer to inspect entities and systems live.

Ideas not built yet: enemy line-of-sight on the minimap, pickups, sounds, real models/textures, instanced rendering if enemy counts go much higher.
