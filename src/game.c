#include "game.h"
#include "config.h"
#include "combat.h"
#include "render.h"
#include "utils.h"
#include "raymath.h"

// Enemies placed when a level loads. Add entries to spawn more
static const EnemyType levelEnemies[] = {
    ENEMY_TYPE_BRUTE,
    ENEMY_TYPE_GUNNER,
};

//----------------------------------------------------------------------------------
// Setup
//----------------------------------------------------------------------------------
static void GiveStartingLoadout(Player *player)
{
    // Slots 5-7 are left empty for future weapons/pickups
    Inventory_SetSlot(&player->inventory, 0, WEAPON_PISTOL);
    Inventory_SetSlot(&player->inventory, 1, WEAPON_SHOTGUN);
    Inventory_SetSlot(&player->inventory, 2, WEAPON_RIFLE);
    Inventory_SetSlot(&player->inventory, 3, WEAPON_PLASMA);
    player->inventory.activeSlot = 0;
}

// Picks the room farthest from the player that has not been used yet
static int PickSpawnRoom(const Level *level, Vector3 playerSpawn, const bool *usedRooms)
{
    int bestRoom = -1;
    float bestDistance = -1.0f;

    for (int i = 1; i < level->roomCount; i++)
    {
        if (usedRooms[i]) continue;
        float distance = Vector3Distance(Level_GetRoomCenter(level, i), playerSpawn);
        if (distance > bestDistance)
        {
            bestDistance = distance;
            bestRoom = i;
        }
    }

    return bestRoom;
}

static Vector3 FindEnemySpawn(const Level *level, Vector3 playerSpawn, bool *usedRooms)
{
    int room = PickSpawnRoom(level, playerSpawn, usedRooms);
    if (room >= 0)
    {
        usedRooms[room] = true;
        return Level_GetRoomCenter(level, room);
    }

    // Fallback: any open cell reasonably far from the player
    for (int attempt = 0; attempt < 200; attempt++)
    {
        int x = GetRandomValue(1, level->width - 2);
        int z = GetRandomValue(1, level->depth - 2);
        Vector3 position = Level_CellToWorld(level, x, z);
        if (!Level_IsSolid(level, x, z) && (Vector3Distance(position, playerSpawn) > 20.0f)) return position;
    }

    return playerSpawn;
}

static void SpawnEnemies(Game *game, Vector3 playerSpawn)
{
    bool usedRooms[LEVEL_MAX_ROOMS] = { 0 };
    int enemyCount = (int)(sizeof(levelEnemies)/sizeof(levelEnemies[0]));

    for (int i = 0; i < enemyCount; i++)
    {
        Vector3 position = FindEnemySpawn(&game->level, playerSpawn, usedRooms);
        Enemies_Spawn(&game->enemies, levelEnemies[i], position, Util_RandomFloat(-PI, PI));
    }
}

void Game_LoadLevel(Game *game, unsigned int seed)
{
    game->seed = seed;
    Level_Generate(&game->level, Level_DefaultSettings(seed));

    Vector3 playerSpawn = Level_GetRoomCenter(&game->level, 0);
    Player_Init(&game->player, playerSpawn, 0.0f);
    GiveStartingLoadout(&game->player);

    Enemies_Clear(&game->enemies);
    Projectiles_Clear(&game->projectiles);
    Effects_Clear(&game->effects);
    SpawnEnemies(game, playerSpawn);

    Player_UpdateCamera(&game->player, &game->camera);
}

void Game_Init(Game *game, unsigned int seed)
{
    *game = (Game){ 0 };
    game->camera.fovy = CAMERA_FOV;
    game->camera.projection = CAMERA_PERSPECTIVE;

    Minimap_Init(&game->minimap, Minimap_DefaultSettings());
    Hud_Init(&game->hud);
    Game_LoadLevel(game, seed);
}

//----------------------------------------------------------------------------------
// Frame
//----------------------------------------------------------------------------------
static void UpdateGlobalInput(Game *game)
{
    if (IsKeyPressed(KEY_F1)) game->hud.showHelp = !game->hud.showHelp;
    if (IsKeyPressed(KEY_M)) Minimap_ToggleExpanded(&game->minimap);
    if (IsKeyPressed(KEY_F5)) Game_LoadLevel(game, game->seed + 1);
    if (!game->player.isAlive && IsKeyPressed(KEY_ENTER)) Game_LoadLevel(game, game->seed);
}

void Game_Update(Game *game, float deltaTime)
{
    deltaTime = fminf(deltaTime, MAX_FRAME_TIME);

    UpdateGlobalInput(game);

    CombatWorld world = (CombatWorld){
        .level = &game->level,
        .player = &game->player,
        .enemies = &game->enemies,
        .projectiles = &game->projectiles,
        .effects = &game->effects,
    };

    PlayerInput input = Player_ReadInput();
    Player_Update(&game->player, &input, &game->level, deltaTime);

    CombatResult shots = Combat_UpdatePlayerWeapon(world, &input);
    Enemies_Update(&game->enemies, &game->level, &game->player, &game->projectiles, deltaTime);
    CombatResult projectileHits = Combat_UpdateProjectiles(world, deltaTime);

    Effects_Update(&game->effects, deltaTime);
    Hud_Update(&game->hud, deltaTime);
    Hud_RegisterHits(&game->hud, shots.hits + projectileHits.hits, shots.kills + projectileHits.kills);

    Player_UpdateCamera(&game->player, &game->camera);
}

void Game_Draw(const Game *game)
{
    FogSettings fog = Render_DefaultFog();

    BeginDrawing();
        ClearBackground(fog.color);

        Render_SetView(game->camera.position, fog);
        BeginMode3D(game->camera);
            Level_Draw(&game->level);
            Enemies_Draw(&game->enemies);
            Projectiles_Draw(&game->projectiles);
            Effects_Draw(&game->effects);
        EndMode3D();

        Hud_Draw(&game->hud, (HudContext){
            .player = &game->player,
            .enemies = &game->enemies,
            .level = &game->level,
            .minimap = &game->minimap,
            .camera = game->camera,
            .seed = game->seed,
        });
    EndDrawing();
}
