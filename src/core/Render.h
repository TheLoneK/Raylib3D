#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Shared drawing helpers (fog, quads, winding-safe triangles)
//----------------------------------------------------------------------------------
namespace Render
{
    struct FogSettings
    {
        Color color;        // Color everything fades toward (also used as the sky)
        float start;        // Distance where fog begins
        float end;          // Distance where fog is fully opaque
    };

    FogSettings DefaultFog();
    void SetView(Vector3 viewPosition, const FogSettings &fog);    // Call once per frame before drawing 3D
    Vector3 GetViewPosition();
    bool IsFullyFogged(Vector3 worldPosition, float radius);        // Cheap distance cull
    Color ApplyFog(Color color, Vector3 worldPosition);             // Tint a color by its distance from the view
    Color Shade(Color color, float factor);                         // Multiply RGB by factor (alpha untouched)

    // Draw a flat quad: corners origin, origin+u, origin+u+v, origin+v. Front face normal = u x v
    void DrawQuad3D(Vector3 origin, Vector3 edgeU, Vector3 edgeV, Color color);

    // Draw a 2D triangle regardless of the vertex winding
    void DrawTriangle2D(Vector2 a, Vector2 b, Vector2 c, Color color);
}
