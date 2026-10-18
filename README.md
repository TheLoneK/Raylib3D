# Raylib3D
simple setup of a 3d raylib game for me to play around with

A first person shooter framework in C++20 + raylib, using [flecs](https://www.flecs.dev) as an entity component system (ECS). It has:

- A generated 70x70 level with key doors and switch doors.
- A crowd of enemies that path toward the player with a shared flow field.
- Two floating bosses, picked at random per level: the Skull Lord and the Lich.
- Hitscan, projectile, melee and charged-bow weapons in a 7 slot inventory with per-weapon ammo/reloading.
- Aiming down sights and headshots.
- Ammo, health and 2x damage pickups.
- A minimap and a HUD.

Killing a boss starts a 20 second countdown to the next level, and your weapons, ammo and health carry over. There is no win condition yet. Notes on what multiplayer would take are in [docs/MULTIPLAYER.md](docs/MULTIPLAYER.md).

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
| Left mouse | Fire (hold for automatic weapons; hold and release to shoot the bow) |
| Right mouse | Aim down sights |
| R | Reload |
| E | Use a wall switch |
| 1 - 7, mouse wheel | Select weapon slot |
| Q | Previous weapon |
| M | Toggle expanded map |
| F1 | Toggle help |
| F2 | God mode (debug) |
| F3 | Spawn another wave of enemies |
| F4 | Make every enemy hunt you |
| F6 | Spawn a random boss in front of you |
| F5 | Generate a new level |
| Enter | Restart the level after dying |

## Code layout

```
src/
  main.cpp          window + main loop only
  core/             Config.h (tuning), MathUtils, Render (fog/quads), RenderAssets (shared GPU meshes),
                    MessageFeed (on-screen notices)
  ecs/              Components.h, GameContext.h, Ecs (component registration, Lifetime system)
  world/            Level (tiles, doors, collision, raycasts), LevelGenerator, FlowField (pathing),
                    SpatialGrid (neighbor queries), LinkTags (door/key/switch colors)
  player/           Player (movement, camera, health), PlayerInput
  weapons/          Weapon (definition table + ammo/reload state), Inventory (7 slots, switching)
  enemies/          EnemyDefs (stats table), Enemies (spawn/damage/headshots/alerts/drops), EnemySystems (AI, crowd, drawing)
  bosses/           Bosses (shared movement, spawning, random boss), SkullLord, Lich
  combat/           PlayerCombat (shooting + melee), Projectiles, Effects, DamageZones
  items/            Pickups (ammo/health/boost/keys), Interactions (switches, key doors)
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
| Boss | the enemy components (without `EnemyBrain`) + `Boss` + its attack component (`SkullLordAttacks` / `LichAttacks`) |
| Lich crystal | the enemy components (without `EnemyBrain`) + `LichCrystal` + `Anchored` |
| Projectile | `WorldTransform`, `Velocity`, `Projectile`, `Lifetime` |
| Pickup / key | `WorldTransform`, `Pickup` |
| Wall switch | `WorldTransform`, `WallSwitch` |
| Damage zone | `WorldTransform`, `DamageZone`, `Lifetime` |
| Tracer / impact | `Tracer` or `Impact`, plus `Lifetime` |

Each frame `Game::Update` does the following:

1. Moves the player and animates doors.
2. Handles switches and key doors.
3. Updates the flow field.
4. Handles shooting.
5. Calls `world.progress()`, which runs the update systems in order:
   1. `EnemyAI`: sight checks (staggered, 5 per second per enemy), the idle / wander / chase / attack state machine, and following the flow field.
   2. `BuildEnemyGrid`: puts living enemies into the spatial grid.
   3. `EnemySeparation`: pushes overlapping enemies apart (and out of the player).
   4. `EnemyVisuals`, then `BossMovement`, `SkullLordAttacks`, `LichAbilities`, `LichCrystals`.
   5. `CollectPickups`, `MoveProjectiles`, `DamageZones`.
   6. `Lifetime`: destroys expired entities.

Draw systems are registered without a phase and run by `Game::Draw` inside `BeginMode3D`/`EndMode3D`. Translucent ones (damage zones) are registered last.

Systems get the non-ECS state (level, player, flow field...) through the `GameContext` singleton: `Ecs::GetContext(it)`.

**Doors and link tags**: doors are part of the level grid (`TileType::Door` + `Level::Door`), because walls, collision, raycasts and pathfinding all read the grid. Each door has a link tag: an id plus a color from `LinkTags`. A key pickup or `WallSwitch` entity with the same id opens it. Closed doors block movement, line of sight and the flow field. When a door opens, `Level::GetNavigationVersion()` changes and the flow field rebuilds, so enemies behind it start pathing through.

**Level generator** (`LevelGenerator`), in order:

1. Places rooms and orders them into a nearest-neighbor chain.
2. Carves corridors along the chain.
3. Picks some chain edges to lock.
4. Puts each door at a corridor chokepoint in front of the locked room.
5. Flood-fills to drop any door that crossing corridors let you walk around.
6. Puts each key or switch somewhere reachable before its own door.

The boss sits in the last room, behind the last door. `LevelGenSettings::Small` keeps the original 40x40 arena.

**Pathfinding**: `FlowField` runs one shortest-path search outward from the player's cell whenever the player changes cell. Each enemy then just looks up "which neighbor cell next?" for its own cell, so 200 enemies cost the same as one search. When an enemy can see the player up close it walks straight at them instead.

**Weapons and damage**: damage = weapon damage × `Config::PlayerWeaponDamageScale` (1.3) × the damage boost × the weapon's `headshotMultiplier` on a headshot.
- A hit is a headshot when it lands above the enemy's `headHeightFraction`, which is set per enemy in `EnemyDefs.cpp`.
- `aimFov` sets each weapon's zoom while aiming; 0 means the weapon can't aim.
- The bow (`FireMode::Charge`) must be held for `minChargeTime` (0.5s) before a release fires anything. It reaches full power at `fullChargeTime`. Anything short of full power arcs with `arcGravity`, and a full draw flies straight.

**Bosses**: each level's boss is chosen at random (`Bosses::SpawnRandom`).
- **Skull Lord**: fires a 3-bolt volley every 2s and drops damage zones around you.
- **Lich**:
  - Fires a burst of 3 soul bolts from its staff every 3s while it can see you; the staff glows just before.
  - Summons 4 hunting enemies every 12s.
  - Drops rings of damage zones.
  - Every 14s, puts a zone under you every 0.5s for 5s.
  - At half health it becomes `Invulnerable` and raises 3 Soul Crystals. Destroying all of them drops the shield.

All the tuning lives in constants at the top of `bosses/SkullLord.cpp` and `bosses/Lich.cpp`.

## Extending

- **New weapon**: add a value to `WeaponType` (`weapons/Weapon.h`) and its entry in `s_weaponDefs` (`weapons/Weapon.cpp`). Give it with `Inventory::GiveWeapon` / `SetSlot` (see `Game::GiveStartingLoadout`).
- **New pickup**: add a value to `PickupType` (`items/PickupTypes.h`), then handle it in `Apply` and `DrawPickup` (`items/Pickups.cpp`).
- **New door behavior**: add a value to `DoorLock` (`world/Level.h`) and handle it in `Interactions::UpdateDoors`.
- **New boss**: add a `BossType` and an `EnemyType`, write `bosses/<Name>.h/.cpp` with `Spawn` + `RegisterSystems` like `Lich`, and add it to `Bosses::Spawn` / `Bosses::RegisterSystems`. Random spawning then includes it automatically.
- **New enemy**: add a value to `EnemyType` (`enemies/EnemyDefs.h`) and its entry in `s_enemyDefs`. Melee vs ranged comes from `attackKind`. Spawn it with `Enemies::Spawn`.
- **New entity kind** (pickups, barrels...): add components to `ecs/Components.h`, register them in `Ecs::RegisterComponents`, and write a `RegisterSystems(world, drawSystems)` function in a feature folder; call it from the `Game` constructor.
- **Enemy count / behavior / level size**: `core/Config.h` (`UseLargeLevel`, `EnemySpawnCount`, `GunnerFraction`, `EnemiesHuntOnSpawn`).
- **Debugging the ECS**: set `EnableFlecsExplorer = true` in `core/Config.h`, run the game and open https://www.flecs.dev/explorer to inspect entities and systems live.

Ideas not built yet: a win condition, sounds, real models/textures, fog of war on the minimap, instanced rendering if enemy counts go much higher.
