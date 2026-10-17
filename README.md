# Raylib3D
simple setup of a 3d raylib game for me to play around with

A small first person shooter framework in C99 + raylib: a generated level with collisions, two enemy types with simple AI, hitscan and projectile weapons, a 7 slot inventory with per-weapon ammo/reloading, a minimap and a HUD. There is no win condition yet.

## Building

raylib stays local to the project: headers live in `include/`, the library goes in `lib/`.

```powershell
# Windows (PowerShell, MinGW gcc) - builds and runs
./run.ps1
```

```bash
# Or with make (mingw32-make on Windows)
make run
```

Every `.c` file in `src/` is compiled automatically, so new modules need no build changes.

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
| F5 | Generate a new level |
| Enter | Restart the level after dying |

## Code layout

`main.c` only owns the window and main loop. Everything else is in `src/`:

| Module | Responsibility |
| --- | --- |
| `game` | Owns every system, runs update/draw each frame, spawns enemies |
| `level` | Room + corridor generation, wall collision (`Level_ResolveCircle`), raycasts and line of sight |
| `player` | Input snapshot (`PlayerInput`), movement, camera, health |
| `weapon` | Weapon definition table and per-weapon ammo / cooldown / reload state |
| `inventory` | 7 weapon slots, switching (raise/lower timing) |
| `enemy` | Enemy definition table and the idle / wander / chase / attack state machine |
| `combat` | Player shooting (hitscan + projectiles), projectile movement and hits |
| `projectile` | Projectile pool and drawing |
| `effects` | Tracers and impact puffs |
| `minimap` | Corner map and expanded full map |
| `hud` | Viewmodel, crosshair, hit markers, health, ammo, inventory bar, enemy health bars |
| `render` / `utils` | Fog, quad/triangle helpers, angle and random helpers |
| `config.h` | Window and global settings |

## Extending

- **New weapon**: add a value to `WeaponType` in `weapon.h`, fill its entry in `weaponDefs` in `weapon.c` (damage, pellets, spread, fire mode, magazine size, reserve, reload time, hitscan or projectile...). Give it to the player with `Inventory_GiveWeapon` or `Inventory_SetSlot` (see `GiveStartingLoadout` in `game.c`).
- **New enemy**: add a value to `EnemyType` in `enemy.h` and fill its entry in `enemyDefs` in `enemy.c`. Melee vs ranged is chosen by `attackKind`. Add it to `levelEnemies` in `game.c` to spawn it.
- **New tile type** (doors, half walls...): add it to `TileType` in `level.h` and handle it in `Level_IsSolid` / `Level_Draw` / the minimap.
- **Pickups**: `Weapon_AddAmmo`, `Inventory_GiveWeapon` and `Player_Heal` are ready to be called from a pickup system.
- **Win condition**: `Enemies_CountAlive` is available for a "clear the level" goal.

Ideas not built yet: pathfinding for enemies (they currently move in straight lines and give up when stuck), fog of war on the minimap, sounds, and real models/textures for the viewmodel and enemies.
