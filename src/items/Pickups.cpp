#include "items/Pickups.h"
#include "core/MathUtils.h"
#include "core/MessageFeed.h"
#include "core/Render.h"
#include "core/RenderAssets.h"
#include "player/Player.h"
#include "world/LinkTags.h"
#include "raymath.h"
#include <cmath>
#include <string>

namespace
{
    constexpr float CollectRadius = 1.4f;
    constexpr Color HealthColor = { 235, 235, 235, 255 };
    constexpr Color CrossColor = { 220, 40, 40, 255 };
    constexpr Color BoostColor = { 190, 70, 255, 255 };

    int GetDefaultAmount(PickupType type, WeaponType weapon)
    {
        switch (type)
        {
            case PickupType::Ammo: return GetWeaponDef(weapon).ammoPickupAmount;
            case PickupType::Health: return Pickups::HealthAmount;
            case PickupType::DamageBoost: return Pickups::DamageBoostSeconds;
            case PickupType::Key: return 0;
        }
        return 0;
    }

    // Returns true when the pickup was used up
    bool Apply(const Pickup &pickup, Player &player, MessageFeed &messages)
    {
        switch (pickup.type)
        {
            case PickupType::Ammo:
            {
                Weapon *weapon = player.GetInventory().FindWeapon(pickup.weapon);
                const WeaponDef &def = GetWeaponDef(pickup.weapon);
                if (weapon == nullptr) return false;

                int added = weapon->AddAmmo(pickup.amount);
                if (added <= 0)
                {
                    messages.Push(std::string(def.name) + " ammo is full", Fade(WHITE, 0.7f), 1.0f);
                    return false;
                }
                messages.Push("+" + std::to_string(added) + " " + def.name + " ammo", def.color);
                return true;
            }
            case PickupType::Health:
            {
                if (!player.Heal(static_cast<float>(pickup.amount))) return false;
                messages.Push("+" + std::to_string(pickup.amount) + " health", Color{ 120, 230, 140, 255 });
                return true;
            }
            case PickupType::DamageBoost:
            {
                player.AddDamageBoost(static_cast<float>(pickup.amount));
                messages.Push("2X DAMAGE for " + std::to_string(pickup.amount) + "s!", BoostColor);
                return true;
            }
            case PickupType::Key:
            {
                player.AddKey(pickup.link);
                messages.Push(std::string("Picked up the ") + LinkTags::GetName(pickup.link) + " key", LinkTags::GetColor(pickup.link));
                return true;
            }
        }
        return false;
    }

    void DrawPickup(const RenderAssets &assets, const WorldTransform &transform, const Pickup &pickup)
    {
        float time = static_cast<float>(GetTime());
        float spin = time*1.5f + pickup.bobPhase;
        Vector3 center = transform.position + Vector3{ 0.0f, 0.6f + sinf(time*2.0f + pickup.bobPhase)*0.15f, 0.0f };
        auto fog = [&](Color color) { return Render::ApplyFog(color, center); };

        switch (pickup.type)
        {
            case PickupType::Ammo:
            {
                Color color = GetWeaponDef(pickup.weapon).color;
                assets.DrawBox(center, Vector3{ 0.7f, 0.4f, 0.45f }, spin, fog(color));
                assets.DrawBox(center + Vector3{ 0.0f, 0.12f, 0.0f }, Vector3{ 0.74f, 0.1f, 0.49f }, spin, fog(Render::Shade(color, 0.5f)));
            } break;
            case PickupType::Health:
            {
                assets.DrawBox(center, Vector3{ 0.6f, 0.6f, 0.6f }, spin, fog(HealthColor));
                // Three crossing bands show a red cross on every face
                assets.DrawBox(center, Vector3{ 0.62f, 0.18f, 0.62f }, spin, fog(CrossColor));
                assets.DrawBox(center, Vector3{ 0.18f, 0.62f, 0.62f }, spin, fog(CrossColor));
                assets.DrawBox(center, Vector3{ 0.62f, 0.62f, 0.18f }, spin, fog(CrossColor));
            } break;
            case PickupType::DamageBoost:
            {
                assets.DrawSphere(center, 0.3f, fog(BoostColor));
                assets.DrawSphere(center, 0.45f, Fade(fog(BoostColor), 0.3f));
                for (int i = 0; i < 3; i++)
                {
                    float angle = spin*2.0f + static_cast<float>(i)*2.0944f;
                    assets.DrawSphere(center + Vector3{ cosf(angle)*0.6f, sinf(angle*1.3f)*0.2f, sinf(angle)*0.6f }, 0.08f, fog(WHITE));
                }
            } break;
            case PickupType::Key:
            {
                Color color = fog(LinkTags::GetColor(pickup.link));
                Vector3 forward = MathUtils::DirectionFromYaw(spin);
                assets.DrawSphere(center - forward*0.2f, 0.16f, color);                               // Bow
                assets.DrawBox(center + forward*0.1f, Vector3{ 0.07f, 0.07f, 0.5f }, spin, color);     // Shaft
                assets.DrawBox(center + forward*0.3f + Vector3{ 0.0f, -0.08f, 0.0f }, Vector3{ 0.06f, 0.14f, 0.06f }, spin, color);
                assets.DrawBox(center + forward*0.2f + Vector3{ 0.0f, -0.06f, 0.0f }, Vector3{ 0.06f, 0.1f, 0.06f }, spin, color);
                // Faint beam so keys can be spotted across a room
                assets.DrawCylinder(transform.position, 0.06f, 4.0f, Fade(LinkTags::GetColor(pickup.link), 0.25f));
            } break;
        }
    }
}

namespace Pickups
{
    flecs::entity Spawn(flecs::world &world, Vector3 position, PickupType type, WeaponType weapon, int link)
    {
        Pickup pickup = { type, weapon, GetDefaultAmount(type, weapon), link, MathUtils::RandomFloat(0.0f, 6.0f) };
        return world.entity()
            .set(WorldTransform{ Vector3{ position.x, 0.0f, position.z }, 0.0f })
            .set(pickup);
    }

    void SpawnEnemyDrop(flecs::world &world, Vector3 position, bool isBoss)
    {
        constexpr WeaponType ammoTypes[] = { WeaponType::Pistol, WeaponType::Shotgun, WeaponType::Rifle, WeaponType::Plasma };
        auto randomAmmo = [&]() { return ammoTypes[GetRandomValue(0, 3)]; };

        if (isBoss)
        {
            // A small pile of loot around the boss
            for (int i = 0; i < 6; i++)
            {
                Vector3 offset = MathUtils::DirectionFromYaw(static_cast<float>(i)*1.047f)*1.5f;
                if (i < 4) Spawn(world, position + offset, PickupType::Ammo, randomAmmo());
                else Spawn(world, position + offset, PickupType::Health);
            }
            return;
        }

        int roll = GetRandomValue(0, 99);
        if (roll < 10) Spawn(world, position, PickupType::Ammo, randomAmmo());
        else if (roll < 15) Spawn(world, position, PickupType::Health);
    }

    void RegisterSystems(flecs::world &world, SystemList &drawSystems)
    {
        world.system<const WorldTransform, const Pickup>("CollectPickups")
            .each([](flecs::iter &it, size_t row, const WorldTransform &transform, const Pickup &pickup) {
                const GameContext &context = Ecs::GetContext(it);
                Player &player = *context.player;
                if (!player.IsAlive()) return;
                if (MathUtils::FlatDistance(transform.position, player.GetPosition()) > CollectRadius) return;
                if (fabsf(player.GetPosition().y - transform.position.y) > 2.0f) return;

                if (Apply(pickup, player, *context.messages)) it.entity(row).destruct();
            });

        drawSystems.push_back(world.system<const WorldTransform, const Pickup>("DrawPickups")
            .kind(0)
            .each([](flecs::iter &it, size_t, const WorldTransform &transform, const Pickup &pickup) {
                if (Render::IsFullyFogged(transform.position, 1.0f)) return;
                DrawPickup(*Ecs::GetContext(it).assets, transform, pickup);
            }));
    }
}
