#include "ui/Minimap.h"
#include "core/MathUtils.h"
#include "core/Render.h"
#include "ecs/Components.h"
#include "player/Player.h"
#include "world/Level.h"
#include "world/LinkTags.h"
#include "raymath.h"
#include <algorithm>

void Minimap::Init(flecs::world &world, const MinimapSettings &settings)
{
    m_settings = settings;
    m_livingEnemies = world.query_builder()
        .with<Enemy>()
        .with<WorldTransform>()
        .with<EnemyBrain>()
        .without<Dead>()
        .build();
    m_pickups = world.query_builder().with<WorldTransform>().with<Pickup>().build();
    m_switches = world.query_builder().with<WorldTransform>().with<WallSwitch>().build();
    m_bosses = world.query_builder().with<WorldTransform>().with<Boss>().without<Dead>().build();
    m_crystals = world.query_builder().with<WorldTransform>().with<LichCrystal>().without<Dead>().build();
    m_platforms = world.query_builder().with<WorldTransform>().with<SolidBox>().with<Platform>().build();
}

Vector2 Minimap::MapView::WorldToMap(Vector3 world) const
{
    return Vector2{ center.x + (world.x - focus.x)*scale, center.y + (world.z - focus.z)*scale };
}

Minimap::MapView Minimap::GetMapView(const Level &level, const Player &player) const
{
    float screenWidth = static_cast<float>(GetScreenWidth());
    float screenHeight = static_cast<float>(GetScreenHeight());
    MapView view = {};

    if (m_expanded)
    {
        float size = std::min(screenWidth, screenHeight) - 120.0f;
        float worldSize = static_cast<float>(std::max(level.GetWidth(), level.GetDepth()))*level.GetTileSize();
        view.bounds = Rectangle{ (screenWidth - size)*0.5f, (screenHeight - size)*0.5f, size, size };
        view.focus = level.GetCenter();
        view.scale = size/worldSize;
    }
    else
    {
        float size = m_settings.compactSize;
        view.bounds = Rectangle{ screenWidth - size - m_settings.margin, m_settings.margin, size, size };
        view.focus = player.GetPosition();
        view.scale = (size*0.5f)/m_settings.compactWorldRadius;
    }

    view.center = Vector2{ view.bounds.x + view.bounds.width*0.5f, view.bounds.y + view.bounds.height*0.5f };
    return view;
}

void Minimap::DrawTiles(const MapView &view, const Level &level) const
{
    float tileSize = level.GetTileSize();
    float cellPixels = tileSize*view.scale;

    for (int z = 0; z < level.GetDepth(); z++)
    {
        for (int x = 0; x < level.GetWidth(); x++)
        {
            Vector2 topLeft = view.WorldToMap(Vector3{ static_cast<float>(x)*tileSize, 0.0f, static_cast<float>(z)*tileSize });
            Rectangle cell = Rectangle{ topLeft.x, topLeft.y, cellPixels + 0.5f, cellPixels + 0.5f };
            if (!CheckCollisionRecs(cell, view.bounds)) continue;

            if (!level.IsWall(x, z))
            {
                DrawRectangleRec(cell, m_settings.floorColor);
            }
            else if (!level.IsWall(x + 1, z) || !level.IsWall(x - 1, z) || !level.IsWall(x, z + 1) || !level.IsWall(x, z - 1))
            {
                DrawRectangleRec(cell, m_settings.wallColor);   // Only walls that border open space
            }
        }
    }
}

void Minimap::DrawEnemies(const MapView &view) const
{
    m_livingEnemies.each([&](flecs::entity entity) {
        const WorldTransform &transform = entity.get<WorldTransform>();
        const EnemyBrain &brain = entity.get<EnemyBrain>();
        Color color = GetEnemyDef(entity.get<Enemy>().type).color;
        Vector2 position = view.WorldToMap(transform.position);

        DrawCircleV(position, m_expanded ? 3.0f : 4.0f, color);
        if (brain.state == EnemyState::Attack) DrawCircleLinesV(position, 6.0f, RED);
    });
}

void Minimap::DrawPlatforms(const MapView &view) const
{
    m_platforms.each([&](flecs::entity entity) {
        Vector3 position = entity.get<WorldTransform>().position;
        Vector3 size = entity.get<SolidBox>().size;
        Vector2 corner = view.WorldToMap(position - Vector3{ size.x*0.5f, 0.0f, size.z*0.5f });
        Vector2 extent = Vector2{ size.x*view.scale, size.z*view.scale };
        DrawRectangleV(corner, extent, Fade(entity.get<Platform>().color, 0.8f));
        DrawRectangleLinesEx(Rectangle{ corner.x, corner.y, extent.x, extent.y }, 1.0f, Fade(WHITE, 0.6f));
    });
}

void Minimap::DrawObjectives(const MapView &view, const Level &level) const
{
    float cellPixels = level.GetTileSize()*view.scale;

    for (const Level::Door &door : level.GetDoors())
    {
        if (door.openAmount >= 1.0f) continue;
        Vector2 center = view.WorldToMap(level.CellToWorld(door.cell));
        Vector2 size = door.spansX ? Vector2{ cellPixels, std::max(3.0f, cellPixels*0.3f) } : Vector2{ std::max(3.0f, cellPixels*0.3f), cellPixels };
        DrawRectangleV(center - size*0.5f, size, LinkTags::GetColor(door.link));
    }

    m_switches.each([&](flecs::entity entity) {
        const WallSwitch &wallSwitch = entity.get<WallSwitch>();
        Vector2 position = view.WorldToMap(entity.get<WorldTransform>().position);
        DrawRectangleV(position - Vector2{ 3.0f, 3.0f }, Vector2{ 6.0f, 6.0f }, wallSwitch.activated ? GRAY : LinkTags::GetColor(wallSwitch.link));
    });

    m_pickups.each([&](flecs::entity entity) {
        const Pickup &pickup = entity.get<Pickup>();
        if (pickup.type != PickupType::Key) return;    // Only keys; ammo/health would clutter the map
        Vector2 position = view.WorldToMap(entity.get<WorldTransform>().position);
        DrawCircleV(position, 4.0f, LinkTags::GetColor(pickup.link));
        DrawCircleLinesV(position, 6.0f, WHITE);
    });

    m_bosses.each([&](flecs::entity entity) {
        Vector2 position = view.WorldToMap(entity.get<WorldTransform>().position);
        DrawCircleV(position, 7.0f, Color{ 170, 70, 255, 255 });
        DrawCircleLinesV(position, 9.0f, WHITE);
    });

    m_crystals.each([&](flecs::entity entity) {
        Vector2 position = view.WorldToMap(entity.get<WorldTransform>().position);
        DrawPoly(position, 4, 6.0f, 45.0f, Color{ 90, 230, 255, 255 });
    });
}

void Minimap::DrawPlayerArrow(const MapView &view, const Player &player) const
{
    Vector2 position = view.WorldToMap(player.GetPosition());
    Vector3 facing = MathUtils::DirectionFromYaw(player.GetYaw());
    Vector2 forward = Vector2{ facing.x, facing.z };
    Vector2 side = Vector2{ -forward.y, forward.x };

    Vector2 tip = position + forward*9.0f;
    Vector2 left = position - forward*6.0f + side*6.0f;
    Vector2 right = position - forward*6.0f - side*6.0f;
    Render::DrawTriangle2D(tip, left, right, m_settings.playerColor);
}

void Minimap::Draw(const Level &level, const Player &player) const
{
    MapView view = GetMapView(level, player);

    DrawRectangleRec(view.bounds, m_settings.backgroundColor);

    BeginScissorMode(static_cast<int>(view.bounds.x), static_cast<int>(view.bounds.y), static_cast<int>(view.bounds.width), static_cast<int>(view.bounds.height));
        DrawTiles(view, level);
        DrawPlatforms(view);
        DrawObjectives(view, level);
        if (m_settings.showEnemies) DrawEnemies(view);
        DrawPlayerArrow(view, player);
    EndScissorMode();

    DrawRectangleLinesEx(view.bounds, 2.0f, m_settings.borderColor);
    DrawText(m_expanded ? "MAP  [M] close" : "MAP [M]", static_cast<int>(view.bounds.x) + 6, static_cast<int>(view.bounds.y + view.bounds.height) + 4, 10, m_settings.borderColor);
}
