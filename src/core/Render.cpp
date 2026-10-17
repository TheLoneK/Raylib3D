#include "core/Render.h"
#include "raymath.h"

namespace Render
{
    static Vector3 s_viewPosition = { 0.0f, 0.0f, 0.0f };
    static FogSettings s_fog = { { 0, 0, 0, 255 }, 0.0f, 1.0f };

    FogSettings DefaultFog()
    {
        return FogSettings{ Color{ 32, 34, 44, 255 }, 12.0f, 85.0f };
    }

    void SetView(Vector3 viewPosition, const FogSettings &fog)
    {
        s_viewPosition = viewPosition;
        s_fog = fog;
    }

    Vector3 GetViewPosition()
    {
        return s_viewPosition;
    }

    bool IsFullyFogged(Vector3 worldPosition, float radius)
    {
        return (Vector3Distance(s_viewPosition, worldPosition) - radius) > s_fog.end;
    }

    Color ApplyFog(Color color, Vector3 worldPosition)
    {
        float distance = Vector3Distance(s_viewPosition, worldPosition);
        float amount = Clamp((distance - s_fog.start)/(s_fog.end - s_fog.start), 0.0f, 1.0f);
        Color fogged = ColorLerp(color, s_fog.color, amount);
        fogged.a = color.a;
        return fogged;
    }

    Color Shade(Color color, float factor)
    {
        return Color{
            static_cast<unsigned char>(Clamp(static_cast<float>(color.r)*factor, 0.0f, 255.0f)),
            static_cast<unsigned char>(Clamp(static_cast<float>(color.g)*factor, 0.0f, 255.0f)),
            static_cast<unsigned char>(Clamp(static_cast<float>(color.b)*factor, 0.0f, 255.0f)),
            color.a
        };
    }

    void DrawQuad3D(Vector3 origin, Vector3 edgeU, Vector3 edgeV, Color color)
    {
        Vector3 a = origin;
        Vector3 b = origin + edgeU;
        Vector3 c = b + edgeV;
        Vector3 d = origin + edgeV;

        DrawTriangle3D(a, b, c, color);
        DrawTriangle3D(a, c, d, color);
    }

    void DrawTriangle2D(Vector2 a, Vector2 b, Vector2 c, Color color)
    {
        // raylib wants counter-clockwise order on screen (y axis points down)
        float cross = (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
        if (cross > 0.0f) DrawTriangle(a, c, b, color);
        else DrawTriangle(a, b, c, color);
    }
}
