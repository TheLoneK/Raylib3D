#pragma once

#include "flecs.h"
#include "raylib.h"
#include "ecs/Ecs.h"
#include <string>

class Level;
class Player;
class MessageFeed;
struct PlayerInput;

//----------------------------------------------------------------------------------
// Doors, keys and wall switches.
//   Key doors open when the player walks up holding the key with the same link tag.
//   Switch doors open when a WallSwitch entity with the same link tag is used (E).
//----------------------------------------------------------------------------------
class Interactions
{
public:
    static constexpr float SwitchReach = 2.6f;
    static constexpr float DoorReach = 3.4f;

    static flecs::entity SpawnSwitch(flecs::world &world, Vector3 position, Vector3 normal, int link);
    static void RegisterSystems(flecs::world &world, SystemList &drawSystems);

    void Init(flecs::world &world);
    void Update(Level &level, const Player &player, MessageFeed &messages, const PlayerInput &input);
    const std::string &GetPrompt() const { return m_prompt; }   // e.g. "[E] Use switch", empty when nothing is in reach

private:
    void UpdateSwitches(Level &level, const Player &player, MessageFeed &messages, const PlayerInput &input);
    void UpdateDoors(Level &level, const Player &player, MessageFeed &messages);

    flecs::query<> m_switches;
    std::string m_prompt;
};
