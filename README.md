# Raylib3D
simple setup of a 3d raylib game for me to play around with

A first person shooter framework in C++20 + raylib, using [flecs](https://www.flecs.dev) as an entity component system (ECS). It has:

- A generated 70x70 level with key doors and switch doors.
- A crowd of enemies that path toward the player with a shared flow field.
- Two floating bosses, picked at random per level: the Skull Lord and the Lich.
- Hitscan, projectile, melee and charged-bow weapons in a 7 slot inventory with per-weapon ammo/reloading.
- Aiming down sights and headshots.
- Ammo, health and 2x damage pickups.
- Elevators and moving platforms the player can ride; some wait for a wall switch.
- Wall switches that send a trigger event to whatever is wired to their channel (doors, platforms...).
- A minimap and a HUD.
- An asset editor (`Editor/`) for importing your own models and textures and putting them on the enemies, bosses and weapons.

Killing a boss starts a 20 second countdown to the next level, and your weapons, ammo and health carry over. There is no win condition yet. Notes on what multiplayer would take are in [docs/MULTIPLAYER.md](docs/MULTIPLAYER.md).

## Building

Requirements: CMake 3.16+ and a MinGW gcc/g++ that supports C++20 (GCC 10 or newer).

```powershell
./run.ps1            # configure (first time), build what changed, run
./run.ps1 -NoRun     # build only
./run.ps1 -Debug     # debug build in build/Debug
./run.ps1 -Clean     # wipe build/ and start over
./run.ps1 -Editor    # build, then open the asset editor instead of the game
```

If PowerShell blocks the script, see `UsefulScripts.md`.

- **raylib**: if `lib/` contains your prebuilt `libraylib.a` (headers in `include/`) it is used. Otherwise CMake downloads and builds raylib 6.2 automatically (needs git, first build only).
- **flecs** lives in `third_party/flecs/` and is compiled once as a static library. The first build takes a minute because of it; after that only changed files rebuild.
- Every `.cpp` under `src/` is picked up automatically, so new files and folders need no build changes.
- The game is built as `TestGame.exe` and the asset editor as `AssetEditor.exe`, both in the project root.

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
| F7 | Spawn an elevator in front of you |
| F8 | Reload the asset library (after saving in the asset editor) |
| F5 | Generate a new level |
| Enter | Restart the level after dying |

## Custom models and textures

Run `./run.ps1 -Editor` to open the asset editor. Import models and textures into `Assets/` (button, or drag files onto the window), pick an enemy or weapon on the right, press **Use selected**, line it up in the viewport, and **Save**. The game reads `Assets/library.ini` at startup; press **F8** in a running game to reload it.

Models: `.glb .gltf .obj .iqm .vox .m3d` (glTF skeletal animations play). Textures: `.png .jpg .bmp .tga .gif .qoi .psd .hdr .dds`. Anything without a model keeps its built-in look. See [Editor/README.md](Editor/README.md) for details.

## Code layout

```
src/
  main.cpp          window + main loop only
  core/             Config.h (tuning), MathUtils, Render (fog/quads), RenderAssets (shared GPU meshes),
                    MessageFeed (on-screen notices)
  ecs/              Components.h, GameContext.h, Ecs (component registration, Lifetime system)
  world/            Level (tiles, doors, collision, raycasts), LevelGenerator, FlowField (pathing),
                    SpatialGrid (neighbor queries), LinkTags (door/key/switch colors),
                    SolidBody (boxes the player stands on / collides with)
  player/           Player (movement, camera, health), PlayerInput
  weapons/          Weapon (definition table + ammo/reload state), Inventory (7 slots, switching)
  enemies/          EnemyDefs (stats table), Enemies (spawn/damage/headshots/alerts/drops), EnemySystems (AI, crowd, drawing)
  bosses/           Bosses (shared movement, spawning, random boss), SkullLord, Lich
  combat/           PlayerCombat (shooting + melee), Projectiles, Effects, DamageZones
  items/            Pickups (ammo/health/boost/keys), Interactions (switches, key doors, door openers),
                    SwitchTypes (switch spawn data, SwitchMode)
  platforms/        PlatformTypes (spawn data + Elevator/Shuttle presets), PlatformPath (waypoint following),
                    Platforms (spawning, movement/carry systems, switch reaction, drawing)
  triggers/         Triggers (send a channel to its receivers), TriggerChannels (channel numbering)
  assets/           AssetSlots (which enemies/weapons can be skinned), AssetManifest (Assets/library.ini),
                    AssetLibrary (loads and draws imported models: fog, light, GPU skinning). Shared with the editor
  ui/               Hud, Minimap, ViewModel3D (imported first person weapon models)
  game/             Game: owns everything and runs a frame
Editor/             Asset editor (its own executable, see Editor/README.md)
Assets/             Imported models/textures and library.ini (written by the editor)
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
| Door opener (switch receiver) | `TriggerTarget`, `DoorOpener` |
| Damage zone | `WorldTransform`, `DamageZone`, `Lifetime` |
| Moving platform / elevator | `WorldTransform`, `Velocity`, `SolidBox`, `PathFollower`, `Platform` (+ `TriggerTarget` when a switch runs it) |
| Tracer / impact | `Tracer` or `Impact`, plus `Lifetime` |

Each frame `Game::Update` does the following:

1. Moves the player (colliding with last frame's platform boxes) and animates doors.
2. Handles switches and key doors.
3. Updates the flow field.
4. Handles shooting.
5. Calls `world.progress()`, which runs the update systems in order:
   1. `EnemyAI`: sight checks (staggered, 5 per second per enemy), the idle / wander / chase / attack state machine, and following the flow field.
   2. `BuildEnemyGrid`: puts living enemies into the spatial grid.
   3. `EnemySeparation`: pushes overlapping enemies apart (and out of the player and platforms).
   4. `EnemyVisuals`, then `BossMovement`, `SkullLordAttacks`, `LichAbilities`, `LichCrystals`.
   5. `CollectPickups`, `MovePlatforms` (also carries or pushes the player), `CollectSolidBodies` (rebuilds the platform box list), `MoveProjectiles`, `DamageZones`.
   6. `Lifetime`: destroys expired entities.

Draw systems are registered without a phase and run by `Game::Draw` inside `BeginMode3D`/`EndMode3D`. Translucent ones (damage zones) are registered last.

Systems get the non-ECS state (level, player, flow field...) through the `GameContext` singleton: `Ecs::GetContext(it)`.

**Doors and link tags**: doors are part of the level grid (`TileType::Door` + `Level::Door`), because walls, collision, raycasts and pathfinding all read the grid. Each door has a link tag: an id plus a color from `LinkTags`. A key pickup with the same id opens it, or a switch door is opened by a `DoorOpener` receiver listening on that id (see triggers below). Closed doors block movement, line of sight and the flow field. When a door opens, `Level::GetNavigationVersion()` changes and the flow field rebuilds, so enemies behind it start pathing through.

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

**Moving platforms**: a platform is a `SolidBox` (an axis aligned box; `WorldTransform.position` is the center of its top face) that a `PathFollower` moves through up to 8 stops, pausing `wait` seconds at each, in `Loop` or `PingPong` order.
- The player doesn't know about flecs. `CollectSolidBodies` copies every `SolidBox` into a `SolidBodyList` (`world/SolidBody.h`) that `Player::Update` collides against: it lands on tops, steps up onto anything lower than `SolidBodies::StepHeight` (0.35), bumps its head on bottoms and slides along sides.
- The player remembers which body it stands on (`GetGroundBody`). After `MovePlatforms` moves a platform it calls `Player::OnBodyMoved`, which carries the player along if they are standing on it, and otherwise pushes them out of the way or lifts them onto the top.
- Stepping or jumping off keeps the platform's velocity, so jumping from a rising elevator goes higher.
- The generator places `elevatorCount` elevators (columns in room corners that rise from the floor to 3 units) and `shuttleCount` shuttles (slabs that board at floor level in a big room, rise, and loop around it 2.5 units up). Keys and pickups stay out of their cells.
- Platforms block everything else too. `Game` attaches the list to the level (`Level::SetSolidBodies`), so `Level::Raycast` and `HasLineOfSight` hit them: hitscan, projectiles (from both teams), enemy and boss sight, and the HUD's health bars. Arrows that hit a platform don't stick (they would be left floating when it moves).
- Enemies and bosses are pushed out of platforms in `EnemySeparation` / `BossMovement`. They walk around platforms but don't ride them.
- A platform with a `channel` starts stopped and listens on that trigger channel: lever down runs it, lever up stops it where it is. Stopped platforms are drawn darker. `switchedPlatformChance` (50%) of generated platforms get a toggle switch on the wall nearest where you board them.

**Triggers and switches**: a `WallSwitch` doesn't know what it controls. Using it calls `Triggers::Send(world, channel, switch, on)`, which emits a flecs `Triggered` event to every entity with a `TriggerTarget` on that channel. Each feature reacts with an observer on its own components:
- `DoorOpener` (`OpenDoorsOnTrigger`, in `Interactions`): opens the level doors with its link tag. Door switches send their door's link as the channel, and `Game` spawns one opener per switch door.
- `PathFollower` (`RunPlatformsOnTrigger`, in `Platforms`): starts or stops the platform.

`SwitchMode::Once` switches stay down after one use (doors), `SwitchMode::Toggle` switches flip on every press and send `on` = lever down. Channels below `TriggerChannels::FirstFree` (100) are door links; the generator hands out the rest. One switch can drive any number of receivers, and a receiver doesn't care what sent the event. If nothing listens, the player sees "Nothing happens".

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
- **New door behavior**: add a value to `DoorLock` (`world/Level.h`) and handle it in `Interactions::UpdateDoors` (or, for anything switch-driven, add a receiver as above).
- **New boss**: add a `BossType` and an `EnemyType`, write `bosses/<Name>.h/.cpp` with `Spawn` + `RegisterSystems` like `Lich`, and add it to `Bosses::Spawn` / `Bosses::RegisterSystems`. Random spawning then includes it automatically.
- **New enemy**: add a value to `EnemyType` (`enemies/EnemyDefs.h`) and its entry in `s_enemyDefs`. Melee vs ranged comes from `attackKind`. Spawn it with `Enemies::Spawn`.
- **Wire something new to a switch**: give the entity `Triggers::Listen(entity, channel)` plus a component of its own, and register an observer for it, e.g. `world.observer<const TriggerTarget, MyThing>().event<Triggered>().each(...)` (`it.param<Triggered>()` has the channel, sender and on/off). Place a `SwitchSpawn` on the same channel (`features.switches` in the generator, or `Interactions::SpawnSwitch`). See `DoorOpener` for the smallest example.
- **New platform**: build a `PlatformSpawn` (`platforms/PlatformTypes.h`) with a box size, stops and a path mode, or use the `Elevator` / `Shuttle` presets, and pass it to `Platforms::Spawn`. To place them in levels, add them in `Builder::PlacePlatforms` (`world/LevelGenerator.cpp`) or tune `elevatorCount` / `shuttleCount`. A static ledge or crate is just `WorldTransform` + `SolidBox` (add `Platform` to get it drawn); it is solid without any extra code.
- **New entity kind** (pickups, barrels...): add components to `ecs/Components.h`, register them in `Ecs::RegisterComponents`, and write a `RegisterSystems(world, drawSystems)` function in a feature folder; call it from the `Game` constructor.
- **Enemy count / behavior / level size**: `core/Config.h` (`UseLargeLevel`, `EnemySpawnCount`, `GunnerFraction`, `EnemiesHuntOnSpawn`).
- **Debugging the ECS**: set `EnableFlecsExplorer = true` in `core/Config.h`, run the game and open https://www.flecs.dev/explorer to inspect entities and systems live.

Ideas not built yet: a win condition, sounds, real models/textures, fog of war on the minimap, instanced rendering if enemy counts go much higher.
