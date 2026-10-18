#include "world/SolidBody.h"
#include "raymath.h"
#include <algorithm>

namespace
{
    // Circle (XZ) against the box footprint. outPush moves the circle out of the footprint
    bool OverlapsFootprint(Vector3 center, float radius, const BoundingBox &box, Vector3 &outPush)
    {
        float dx = center.x - Clamp(center.x, box.min.x, box.max.x);
        float dz = center.z - Clamp(center.z, box.min.z, box.max.z);
        float distSq = dx*dx + dz*dz;
        if (distSq >= radius*radius) return false;

        outPush = Vector3{ 0.0f, 0.0f, 0.0f };
        if (distSq > 0.000001f)
        {
            float dist = sqrtf(distSq);
            float depth = radius - dist;
            outPush = Vector3{ dx/dist*depth, 0.0f, dz/dist*depth };
        }
        else
        {
            // Center is inside the footprint: leave through the closest side
            float left = center.x - box.min.x, right = box.max.x - center.x;
            float back = center.z - box.min.z, front = box.max.z - center.z;
            float best = std::min({ left, right, back, front });
            if (best == left) outPush.x = -(left + radius);
            else if (best == right) outPush.x = right + radius;
            else if (best == back) outPush.z = -(back + radius);
            else outPush.z = front + radius;
        }
        return true;
    }
}

namespace SolidBodies
{
    Contact ResolveCylinder(CylinderMove &move, const BoundingBox &box)
    {
        Vector3 push;
        if (!OverlapsFootprint(move.feet, move.radius, box, push)) return Contact{ ContactType::None, Vector3{} };

        float top = box.max.y, bottom = box.min.y;

        // Landing: coming down onto the top, or stepping up onto a low one
        if ((move.verticalSpeed <= 0.0f) && (move.previousFeetY >= top - StepHeight) && (move.feet.y <= top + GroundSnap))
        {
            move.feet.y = top;
            return Contact{ ContactType::Landed, Vector3{ 0.0f, 1.0f, 0.0f } };
        }

        if ((move.feet.y >= top) || (move.feet.y + move.height <= bottom)) return Contact{ ContactType::None, Vector3{} };

        // Head bump: rising into the bottom from below
        float belowY = bottom - move.height;
        if ((move.verticalSpeed > 0.0f) && (move.previousFeetY <= belowY + GroundSnap) && (belowY >= move.floorY))
        {
            move.feet.y = belowY;
            return Contact{ ContactType::HitHead, Vector3{ 0.0f, -1.0f, 0.0f } };
        }

        move.feet.x += push.x;
        move.feet.z += push.z;
        return Contact{ ContactType::Side, Vector3Normalize(push) };
    }

    bool PushOut(const SolidBodyList &bodies, Vector3 &feet, float radius, float height)
    {
        bool touched = false;
        for (const SolidBody &body : bodies)
        {
            if ((feet.y >= body.bounds.max.y) || (feet.y + height <= body.bounds.min.y)) continue;

            Vector3 push;
            if (!OverlapsFootprint(feet, radius, body.bounds, push)) continue;
            feet.x += push.x;
            feet.z += push.z;
            touched = true;
        }
        return touched;
    }

    RayCollision Raycast(const SolidBodyList &bodies, Ray ray, float maxDistance, std::uint64_t *outBody)
    {
        RayCollision result = {};
        result.distance = maxDistance;
        for (const SolidBody &body : bodies)
        {
            RayCollision hit = GetRayCollisionBox(ray, body.bounds);
            if (!hit.hit || (hit.distance < 0.0f) || (hit.distance >= result.distance)) continue;

            result = hit;
            if (outBody != nullptr) *outBody = body.id;
        }
        return result;
    }

    BoundingBox BoxFromTop(Vector3 topCenter, Vector3 size)
    {
        Vector3 half = { size.x*0.5f, 0.0f, size.z*0.5f };
        return BoundingBox{
            Vector3{ topCenter.x - half.x, topCenter.y - size.y, topCenter.z - half.z },
            Vector3{ topCenter.x + half.x, topCenter.y, topCenter.z + half.z },
        };
    }

    const SolidBody *Find(const SolidBodyList &bodies, std::uint64_t id)
    {
        auto it = std::find_if(bodies.begin(), bodies.end(), [&](const SolidBody &body) { return body.id == id; });
        return (it != bodies.end()) ? &*it : nullptr;
    }
}
