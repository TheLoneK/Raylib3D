#include "render.h"
#include "raymath.h"

static Vector3 viewPosition = { 0 };
static FogSettings currentFog = { 0 };

FogSettings Render_DefaultFog(void)
{
    return (FogSettings){ .color = (Color){ 32, 34, 44, 255 }, .start = 12.0f, .end = 85.0f };
}

void Render_SetView(Vector3 position, FogSettings fog)
{
    viewPosition = position;
    currentFog = fog;
}

Color Render_ApplyFog(Color color, Vector3 worldPosition)
{
    float distance = Vector3Distance(viewPosition, worldPosition);
    float amount = Clamp((distance - currentFog.start)/(currentFog.end - currentFog.start), 0.0f, 1.0f);
    Color fogged = ColorLerp(color, currentFog.color, amount);
    fogged.a = color.a;
    return fogged;
}

Color Render_Shade(Color color, float factor)
{
    return (Color){
        (unsigned char)Clamp((float)color.r*factor, 0.0f, 255.0f),
        (unsigned char)Clamp((float)color.g*factor, 0.0f, 255.0f),
        (unsigned char)Clamp((float)color.b*factor, 0.0f, 255.0f),
        color.a
    };
}

void Render_DrawQuad3D(Vector3 origin, Vector3 edgeU, Vector3 edgeV, Color color)
{
    Vector3 a = origin;
    Vector3 b = Vector3Add(origin, edgeU);
    Vector3 c = Vector3Add(b, edgeV);
    Vector3 d = Vector3Add(origin, edgeV);

    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(a, c, d, color);
}

void Render_DrawTriangle2D(Vector2 a, Vector2 b, Vector2 c, Color color)
{
    // raylib wants counter-clockwise order on screen (y axis points down)
    float cross = (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
    if (cross > 0.0f) DrawTriangle(a, c, b, color);
    else DrawTriangle(a, b, c, color);
}
