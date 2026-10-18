#pragma once

#include "raylib.h"
#include <cstdint>
#include <vector>

//----------------------------------------------------------------------------------
// Solid bodies: axis aligned boxes the player collides with and can stand on
// (moving platforms, elevators...). The ECS rebuilds the list every frame, so the
// Player collides with them without knowing about flecs.
//----------------------------------------------------------------------------------
struct SolidBody
{
    std::uint64_t id;           // Entity id (never SolidBodies::None)
    BoundingBox bounds;
    Vector3 velocity;           // Units per second; riders keep it when they step or jump off
};

using SolidBodyList = std::vector<SolidBody>;

namespace SolidBodies
{
    inline constexpr std::uint64_t None = 0;
    inline constexpr float StepHeight = 0.35f;      // Tops this close above the feet are stepped onto instead of blocking
    inline constexpr float GroundSnap = 0.05f;      // Feet this close above a top still count as standing on it

    enum class ContactType
    {
        None,
        Landed,         // Feet put on the top face
        HitHead,        // Pushed down out of the bottom face
        Side            // Pushed out sideways (XZ)
    };

    struct Contact
    {
        ContactType type;
        Vector3 normal;             // Direction the cylinder was pushed
    };

    // An upright cylinder taking one step. Speeds and previous positions are relative to the box,
    // so a box that moved into the cylinder is resolved the same way as the cylinder moving into the box
    struct CylinderMove
    {
        Vector3 feet;               // In/out: bottom center
        float radius;
        float height;
        float previousFeetY;        // Feet height before this step
        float verticalSpeed;        // Positive = moving up (jumping)
        float floorY;               // Never pushed below this; pushed sideways instead
    };

    Contact ResolveCylinder(CylinderMove &move, const BoundingBox &box);

    BoundingBox BoxFromTop(Vector3 topCenter, Vector3 size);      // Box whose top face is centered on topCenter
    const SolidBody *Find(const SolidBodyList &bodies, std::uint64_t id);
}
