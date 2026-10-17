# Making Raylib3D multiplayer (thought exercise)

This is a design sketch, not an implementation plan. It covers what making this project multiplayer would take, mapped onto the Unreal concepts you already know.

## The short version

Unreal gives you replication, RPCs, authority roles, movement prediction and relevancy as part of the engine. raylib gives you none of that, and flecs has no networking either. So everything Unreal does for you between "a client pressed fire" and "every screen shows the shot" has to be written here.

The good news is that this codebase already has a few of the hard prerequisites:

- `PlayerInput` is a per-frame input snapshot. That is exactly what you would send to a server.
- The level is fully generated from a seed. Clients can rebuild the world from 4 bytes instead of downloading it.
- The ECS keeps game state as plain data in components, which is what you serialize.
- `SpatialGrid` already answers "what is near this point", which is the core of relevancy.

The main work is a refactor that splits the game into simulation and presentation, replaces the single `Player` with a set of players, and adds a network layer on top.

## Pick a model

| Model | How it works | Fit here |
| --- | --- | --- |
| **Server authoritative** (Unreal's model) | The server runs the real game; clients send input and draw what the server says | **Recommended.** Handles 200 enemies, late joiners and cheating sensibly |
| Deterministic lockstep (RTS style) | Everyone runs the full simulation and only inputs are exchanged | Tiny bandwidth even with 200 enemies, but needs bit-exact floats on every machine (compilers, CPUs, flecs iteration order, RNG), and everyone waits for the slowest connection. Bad fit for a shooter |
| Peer to peer, no authority | Each client owns its own stuff | Simple for 2 players, but enemies and pickups need one owner anyway, so it degrades into the first model |

Go with a **listen server** (the host plays and is the authority) or a **dedicated server**: the same code built without a window. That matches Unreal's `NM_ListenServer` / `NM_DedicatedServer`.

## Unreal concepts and their equivalent here

| Unreal | Here |
| --- | --- |
| `NetDriver` / sockets | A transport library: **ENet** (small, reliable + unreliable UDP channels) or Valve's **GameNetworkingSockets**. raylib has no networking |
| Replicated actor | An entity with a `Replicated { uint32 netId; }` component. flecs entity ids differ per machine, so each side keeps a `netId -> entity` map |
| `UPROPERTY(Replicated)` | The components you choose to serialize into snapshots: `WorldTransform`, `Health`, `EnemyBrain::state` (for animation), `Boss`, `DamageZone`, door state... |
| `ReplicatedUsing=OnRep_X` | The client compares an incoming value to its current one and reacts. For example, a `Dead` flag arriving starts the corpse animation |
| Server RPC | Client-to-server messages. Most "RPCs" are just the input stream (fire, interact); things like "use switch" can ride in `PlayerInput` |
| Multicast RPC | Unreliable events to every relevant client: tracers, impacts, muzzle flashes |
| Client RPC | Reliable message to one client: "Picked up the RED key", hit markers |
| `HasAuthority()` / roles | Register different flecs systems per role: AI, combat and pickups only on the server; draw and interpolation systems only on clients |
| `GameMode` | Server-only rules: spawning, respawns, win condition |
| `GameState` / `PlayerState` | Replicated singletons: level seed, door states, boss engaged, scores, per-player keys |
| `CharacterMovementComponent` | Client-side prediction + server reconciliation for `Player::Update` (see below) |
| Net relevancy / `NetCullDistance` | Query `SpatialGrid` around each client's player and only send nearby entities |
| `NetUpdateFrequency` / priority | Per-entity send rates: the local player every tick, nearby enemies often, far ones rarely |

## What would have to change in this codebase

### 1. Fixed timestep and simulation vs presentation

`Game::Update` currently takes the frame's variable `deltaTime` and does everything. A networked game needs:

- **A fixed simulation tick** (for example 60 Hz) driven by an accumulator, so the server and every client step the same way.
- **The simulation** (level, players, enemy AI, combat, pickups, doors, boss) in one place that can run without a window, which is what a dedicated server needs.
- **The presentation** (`RenderAssets`, draw systems, HUD, minimap, viewmodel, effects) only on clients.

flecs makes the split clean. Register the systems in groups (`RegisterSimulationSystems`, `RegisterPresentationSystems`) and choose which groups to register based on the role. `Game` becomes `ServerGame` + `ClientGame`, sharing the simulation code. `RenderAssets` would have to become optional, since the server has no OpenGL context.

### 2. From one player to many

The single player is assumed in a lot of places:

| Code | Single-player assumption | Multiplayer version |
| --- | --- | --- |
| `GameContext::player` | One `Player*` | Players become entities (`PlayerTag`, `WorldTransform`, `Health`, `Inventory`...) or a `std::vector<Player>` |
| `FlowField` | One target cell | Run the same Dijkstra from **every player's cell at once** (multi-source). Each enemy then walks toward its nearest player, still for the price of one search |
| `EnemyAI` / `BossAI` | Sees and attacks "the player" | Pick a target per enemy (nearest visible player) and store it in `EnemyBrain` |
| `Projectiles`, `DamageZones` | Hurt `context.player` | Test against every player's bounds |
| `CollectPickups`, `Interactions` | Apply to the one player | Apply to whichever player touched it; decide whether keys are personal or shared by the team (shared is friendlier in co-op) |
| `Hud`, `Minimap`, camera | Read the one player | Read the *local* player; draw other players as entities |

### 3. Input as commands, with prediction

Send `PlayerInput` every tick with a sequence number. Two changes to it:

- Send absolute `yaw`/`pitch` instead of `lookDelta`, so a lost packet doesn't lose mouse movement.
- Add `interactPressed`-style one-shot flags with "repeat until acknowledged" semantics, so a dropped packet doesn't eat a key press.

Then do the classic `CharacterMovementComponent` loop:

1. The client applies its input to its own `Player` **immediately** (prediction) and keeps a buffer of unacknowledged inputs.
2. The server applies the same inputs to the authoritative player and returns `{ last processed input sequence, position, velocity, ... }`.
3. The client snaps to the server state and **replays** the inputs the server hasn't processed yet. When prediction was right this is invisible; when it was wrong (bumped by an enemy) it corrects smoothly.

`Player::UpdateMovement` already takes only input, the level and `deltaTime`, which is what replay needs. Weapon switching, reloading and firing animations should also be predicted, so the gun responds instantly.

### 4. Snapshots for everything else

The server sends each client a snapshot 20-30 times a second containing the replicated components of relevant entities.

- **Interpolation**: remote players, enemies and the boss are drawn about 100 ms in the past, blended between the two snapshots around that time. This is Unreal's simulated proxy smoothing. Enemies move in straight lines between waypoints, so linear interpolation is fine.
- **Delta compression**: send only what changed since the last snapshot the client acknowledged.
- **Quantization**: the level is 280 units across, so 16 bits per position axis gives about 4 mm precision. Yaw fits in 8-16 bits, health in 8-16 bits.
- **Bandwidth check**: 200 enemies at roughly 10 bytes each is 2 KB per snapshot before culling. Relevancy (only enemies within ~60 units, which fog hides anyway) and delta compression cut that to a fraction. That's affordable.

flecs can help with serialization. Its meta/reflection addon (the same thing the explorer uses) can describe component members, so a generic serializer could walk them. Hand-written `Serialize(BitWriter&, const WorldTransform&)` functions are simpler and faster for a handful of component types.

### 5. Combat

- **Hitscan**: the client sends "fired" as part of its input. The server traces the ray. To make hits feel fair, use **lag compensation**: the server keeps a short history (about 200 ms) of enemy positions and rewinds them to what that client saw when it fired. In ECS terms that is one more system that records `WorldTransform` into a ring buffer each tick.
- **Projectiles** (plasma, gunner shots, boss bolts): spawned and simulated by the server and replicated as entities. The firing client can spawn a cosmetic copy right away and swap it for the real one when it arrives.
- **Melee** (chainsaw): checked on the server against rewound positions, like hitscan.
- **Cosmetics**: tracers, impacts and hit flashes become unreliable multicast events, or each client derives them from replicated "shot" events. They never affect the simulation.
- **Hit markers / damage numbers**: sent to the shooter when the server confirms the hit.

### 6. World state and events

| State | How it replicates |
| --- | --- |
| Level layout | The seed and settings, once at connect. Clients run `LevelGenerator::Generate` themselves. This requires generation to use its **own RNG**. It currently shares raylib's global `GetRandomValue` stream with gameplay; that's harmless now but fragile once timing differs between machines |
| Doors | Small per-door state (`opening`, open time) in the game-state snapshot, or as reliable "door N opened at tick T" events. Clients animate locally; the server's flow field and collision use the server's copy |
| Pickups, keys, switches | Replicated entities. Consumption is a reliable event, so two players grabbing the same health pack is resolved by the server |
| Boss | A replicated entity plus game-state fields (engaged, health) that drive the HUD bar |
| Messages | Reliable client messages: per player ("Picked up the RED key") or broadcast ("BLUE door opening") |

### 7. Connection lifecycle

- **Handshake**: protocol version check, assign a player id, send the seed/settings, then a full snapshot.
- **Join in progress**: the full snapshot covers enemies, pickups and doors. That's one reason the server-authoritative model is easier than lockstep, which would have to replay history.
- **Disconnect and timeout handling**: remove the player entity, and pick new targets for enemies that were chasing it.
- **NAT traversal and lobbies**: Steam (GameNetworkingSockets/Steamworks) or a relay if you want internet play without port forwarding.

## A plausible order of work

1. **Refactor, still single player**:
   - Fixed timestep.
   - Separate RNG for level generation.
   - Players as a list or entities, with targeting and a multi-source flow field.
   - Split systems into simulation vs presentation.
   - A headless build that runs the simulation without a window.

   This is the biggest chunk, and it is valuable even without networking because it makes the code easier to test.
2. **Transport**:
   - Add ENet via CMake `FetchContent`; `ws2_32` is already linked for flecs.
   - Connect, handshake, seed sync. Two clients see the same level.
3. **Snapshots + interpolation**: remote players and enemies move on everyone's screen; the server owns all AI.
4. **Prediction + reconciliation** for the local player: movement feels instant on a laggy connection.
5. **Combat replication**:
   - Server-side hits, lag compensation for hitscan and melee.
   - Projectile entities, cosmetic events.
6. **World events**: doors, switches, keys, pickups, boss, messages.
7. **Hardening**:
   - Delta compression and quantization.
   - Relevancy using `SpatialGrid`.
   - Join in progress, disconnects.
   - Basic cheat resistance: the server validates fire rate, ammo and movement speed.

Tools that help along the way:

- A simulated-lag mode (delay, jitter and drop packets in the transport) to test without real internet.
- The flecs explorer pointed at the **server** world, to inspect authoritative state while clients run.

## Rough effort

For one developer who knows the concepts from Unreal:

- Steps 1-3 (two players seeing each other move in a shared level): **2-4 weeks**.
- A version that feels good on real connections (prediction, lag compensation, delta compression, join in progress): **another 1-2 months**.

Most of that time goes into the parts Unreal hides: input buffering and replay, interpolation buffers, and making sure server-only and client-only code never reach into each other's data.
