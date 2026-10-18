#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// GPU meshes shared by everything drawn in large numbers (enemies, projectiles...)
// Uploaded once, drawn with a transform each, which is much cheaper than rebuilding
// spheres/cylinders vertex by vertex every frame.
// Needs a window (OpenGL context), so create it after InitWindow.
//----------------------------------------------------------------------------------
class RenderAssets
{
public:
    RenderAssets();
    ~RenderAssets();
    RenderAssets(const RenderAssets &) = delete;
    RenderAssets &operator=(const RenderAssets &) = delete;

    void DrawSphere(Vector3 center, float radius, Color color) const;
    void DrawCylinder(Vector3 base, float radius, float height, Color color) const;                 // Upright
    void DrawCylinderRotated(Vector3 base, float radius, float height, Vector3 axis, float angleDegrees, Color color) const;
    void DrawBox(Vector3 center, Vector3 size, float yaw, Color color) const;                       // Rotated around Y
    void DrawCylinderBetween(Vector3 start, Vector3 end, float radius, Color color) const;           // Any orientation
    void DrawCone(Vector3 base, float radius, float height, Color color) const;                     // Point up (negative height points down)

private:
    Model m_sphere;     // Radius 1, centered
    Model m_cylinder;   // Radius 1, height 1, base at the origin
    Model m_cube;       // Size 1, centered
    Model m_cone;       // Radius 1, height 1, base at the origin
};
