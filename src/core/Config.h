#pragma once

//----------------------------------------------------------------------------------
// Global configuration shared by every module
//----------------------------------------------------------------------------------
namespace Config
{
    inline constexpr int ScreenWidth = 1280;
    inline constexpr int ScreenHeight = 720;
    inline constexpr int TargetFps = 200;
    inline constexpr const char *WindowTitle = "Raylib3D - FPS framework";

    inline constexpr float CameraFov = 80.0f;

    // Largest frame step the simulation will take (prevents tunneling on hitches)
    inline constexpr float MaxFrameTime = 1.0f/30.0f;

    // Player damage scale applied on top of every weapon's damage (balance knob)
    inline constexpr float PlayerWeaponDamageScale = 1.3f;

    // Seconds between killing a boss and loading the next level
    inline constexpr float LevelTransitionTime = 20.0f;

    // Level: true = large level (~3x area) with key/switch doors and the boss, false = the small arena
    inline constexpr bool UseLargeLevel = true;

    // Enemy spawning
    inline constexpr int EnemySpawnCount = 200;         // Spawned when a level loads
    inline constexpr int EnemyWaveSize = 50;            // Spawned with F3
    inline constexpr float GunnerFraction = 0.25f;      // Share of ranged enemies
    inline constexpr bool EnemiesHuntOnSpawn = false;   // true: path to the player immediately (F4 does this at runtime), false: wander until they see/hear you
    inline constexpr float EnemyMinSpawnDistance = 30.0f;

    // Flecs explorer (https://www.flecs.dev/explorer) - connects to the running game for debugging
    inline constexpr bool EnableFlecsExplorer = false;
}
