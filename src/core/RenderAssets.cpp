#include "core/RenderAssets.h"
#include "raymath.h"

RenderAssets::RenderAssets()
{
    m_sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 8, 10));
    m_cylinder = LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, 10));
    m_cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    m_cone = LoadModelFromMesh(GenMeshCone(1.0f, 1.0f, 10));
}

RenderAssets::~RenderAssets()
{
    UnloadModel(m_sphere);
    UnloadModel(m_cylinder);
    UnloadModel(m_cube);
    UnloadModel(m_cone);
}

void RenderAssets::DrawSphere(Vector3 center, float radius, Color color) const
{
    DrawModelEx(m_sphere, center, Vector3{ 0.0f, 1.0f, 0.0f }, 0.0f, Vector3{ radius, radius, radius }, color);
}

void RenderAssets::DrawCylinder(Vector3 base, float radius, float height, Color color) const
{
    DrawModelEx(m_cylinder, base, Vector3{ 0.0f, 1.0f, 0.0f }, 0.0f, Vector3{ radius, height, radius }, color);
}

void RenderAssets::DrawCylinderRotated(Vector3 base, float radius, float height, Vector3 axis, float angleDegrees, Color color) const
{
    DrawModelEx(m_cylinder, base, axis, angleDegrees, Vector3{ radius, height, radius }, color);
}

void RenderAssets::DrawBox(Vector3 center, Vector3 size, float yaw, Color color) const
{
    DrawModelEx(m_cube, center, Vector3{ 0.0f, 1.0f, 0.0f }, yaw*RAD2DEG, size, color);
}

void RenderAssets::DrawCylinderBetween(Vector3 start, Vector3 end, float radius, Color color) const
{
    Vector3 delta = end - start;
    float length = Vector3Length(delta);
    if (length < 0.0001f) return;

    // Rotate the model's +Y axis onto the segment direction
    Vector3 up = { 0.0f, 1.0f, 0.0f };
    Vector3 direction = delta/length;
    Vector3 axis = Vector3CrossProduct(up, direction);
    float angle = acosf(Clamp(Vector3DotProduct(up, direction), -1.0f, 1.0f))*RAD2DEG;
    if (Vector3LengthSqr(axis) < 0.000001f) axis = Vector3{ 1.0f, 0.0f, 0.0f };     // Straight up or down

    DrawModelEx(m_cylinder, start, Vector3Normalize(axis), angle, Vector3{ radius, length, radius }, color);
}

void RenderAssets::DrawCone(Vector3 base, float radius, float height, Color color) const
{
    if (height >= 0.0f) DrawModelEx(m_cone, base, Vector3{ 0.0f, 1.0f, 0.0f }, 0.0f, Vector3{ radius, height, radius }, color);
    else DrawModelEx(m_cone, base, Vector3{ 1.0f, 0.0f, 0.0f }, 180.0f, Vector3{ radius, -height, radius }, color);
}
