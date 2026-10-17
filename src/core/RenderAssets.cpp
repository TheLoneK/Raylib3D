#include "core/RenderAssets.h"
#include "raymath.h"

RenderAssets::RenderAssets()
{
    m_sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 8, 10));
    m_cylinder = LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, 10));
    m_cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
}

RenderAssets::~RenderAssets()
{
    UnloadModel(m_sphere);
    UnloadModel(m_cylinder);
    UnloadModel(m_cube);
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
