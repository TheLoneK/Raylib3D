#include "items/Interactions.h"
#include "core/MathUtils.h"
#include "core/MessageFeed.h"
#include "core/Render.h"
#include "core/RenderAssets.h"
#include "ecs/Components.h"
#include "player/Player.h"
#include "player/PlayerInput.h"
#include "world/Level.h"
#include "triggers/Triggers.h"
#include "world/LinkTags.h"
#include "raymath.h"
#include <string>

flecs::entity Interactions::SpawnSwitch(flecs::world &world, const SwitchSpawn &spawn)
{
    return world.entity()
        .set(WorldTransform{ spawn.position, MathUtils::YawFromDirection(spawn.normal) })
        .set(WallSwitch{ spawn.channel, spawn.normal, spawn.mode, false, spawn.color, spawn.name });
}

flecs::entity Interactions::SpawnDoorOpener(flecs::world &world, int channel, int link)
{
    flecs::entity opener = world.entity().set(DoorOpener{ link });
    Triggers::Listen(opener, channel);
    return opener;
}

void Interactions::RegisterSystems(flecs::world &world, SystemList &drawSystems)
{
    world.observer<const TriggerTarget, const DoorOpener>("OpenDoorsOnTrigger")
        .event<Triggered>()
        .each([](flecs::iter &it, size_t, const TriggerTarget &, const DoorOpener &opener) {
            const GameContext &context = Ecs::GetContext(it);
            int opened = context.level->OpenDoorsWithLink(opener.link);
            if (opened == 0) return;
            std::string text = std::string(LinkTags::GetName(opener.link)) + ((opened == 1) ? " door opening" : " doors opening");
            context.messages->Push(text, LinkTags::GetColor(opener.link));
        });

    drawSystems.push_back(world.system<const WorldTransform, const WallSwitch>("DrawSwitches")
        .kind(0)
        .each([](flecs::iter &it, size_t, const WorldTransform &transform, const WallSwitch &wallSwitch) {
            if (Render::IsFullyFogged(transform.position, 1.0f)) return;
            const RenderAssets &assets = *Ecs::GetContext(it).assets;
            Color linkColor = wallSwitch.color;
            auto fog = [&](Color color) { return Render::ApplyFog(color, transform.position); };

            // Backplate, lever (flips down once used) and a status light
            assets.DrawBox(transform.position, Vector3{ 0.6f, 0.8f, 0.08f }, transform.yaw, fog(Color{ 60, 60, 66, 255 }));
            assets.DrawBox(transform.position + wallSwitch.normal*0.05f, Vector3{ 0.45f, 0.65f, 0.04f }, transform.yaw, fog(Render::Shade(linkColor, 0.6f)));
            float leverY = wallSwitch.activated ? -0.18f : 0.18f;
            assets.DrawBox(transform.position + wallSwitch.normal*0.14f + Vector3{ 0.0f, leverY, 0.0f }, Vector3{ 0.12f, 0.12f, 0.22f }, transform.yaw, fog(Color{ 200, 200, 205, 255 }));
            Color light = wallSwitch.activated ? Color{ 80, 255, 120, 255 } : linkColor;
            assets.DrawSphere(transform.position + wallSwitch.normal*0.08f + Vector3{ 0.0f, 0.52f, 0.0f }, 0.09f, light);
        }));
}

void Interactions::Init(flecs::world &world)
{
    m_switches = world.query_builder()
        .with<WorldTransform>()
        .with<WallSwitch>()
        .build();
}

void Interactions::UpdateSwitches(const Player &player, MessageFeed &messages, const PlayerInput &input)
{
    Vector3 eye = player.GetEyePosition();
    Vector3 forward = player.GetForward();
    flecs::entity target;
    float bestDistance = SwitchReach;

    // The switch must be close, roughly in view, and the player must be on the front side of the wall
    m_switches.each([&](flecs::entity entity) {
        const WorldTransform &transform = entity.get<WorldTransform>();
        const WallSwitch &wallSwitch = entity.get<WallSwitch>();
        Vector3 toSwitch = transform.position - eye;
        float distance = Vector3Length(toSwitch);

        if (distance >= bestDistance) return;
        if (Vector3DotProduct(toSwitch/distance, forward) < 0.5f) return;
        if (Vector3DotProduct(wallSwitch.normal, eye - transform.position) <= 0.0f) return;

        bestDistance = distance;
        target = entity;
    });

    if (!target) return;

    WallSwitch &wallSwitch = target.get_mut<WallSwitch>();
    if (wallSwitch.activated && (wallSwitch.mode == SwitchMode::Once))
    {
        m_prompt = "Switch already used";
        return;
    }

    m_prompt = std::string("[E] Use ") + wallSwitch.name + " switch";
    if (input.interactPressed)
    {
        // The switch only sends its channel; whatever listens on it reacts (and reports)
        wallSwitch.activated = (wallSwitch.mode == SwitchMode::Toggle) ? !wallSwitch.activated : true;
        flecs::world world = target.world();
        if (Triggers::Send(world, wallSwitch.channel, target, wallSwitch.activated) == 0) messages.Push("Nothing happens", Fade(WHITE, 0.7f), 1.5f);
    }
}

void Interactions::UpdateDoors(Level &level, const Player &player, MessageFeed &messages)
{
    const std::vector<Level::Door> &doors = level.GetDoors();

    for (size_t i = 0; i < doors.size(); i++)
    {
        const Level::Door &door = doors[i];
        if (door.opening) continue;
        if (MathUtils::FlatDistance(level.GetDoorCenter(door), player.GetPosition()) > DoorReach) continue;

        std::string color = LinkTags::GetName(door.link);
        if (door.lock == DoorLock::Key)
        {
            if (player.HasKey(door.link))
            {
                level.OpenDoor(static_cast<int>(i));
                messages.Push("Opened with the " + color + " key", LinkTags::GetColor(door.link));
            }
            else
            {
                messages.Push("This door needs the " + color + " key", LinkTags::GetColor(door.link), 1.5f);
            }
        }
        else
        {
            messages.Push("This door is opened by a " + color + " switch", LinkTags::GetColor(door.link), 1.5f);
        }
    }
}

void Interactions::Update(Level &level, const Player &player, MessageFeed &messages, const PlayerInput &input)
{
    m_prompt.clear();
    if (!player.IsAlive()) return;

    UpdateSwitches(player, messages, input);
    UpdateDoors(level, player, messages);
}
