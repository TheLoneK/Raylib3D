#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Link tags connect a door to whatever opens it (a key or a wall switch).
// Matching ids are the actual link; the color is just how the player sees it,
// so a door, its key/switch and their minimap icons always share one color.
//----------------------------------------------------------------------------------
namespace LinkTags
{
    inline constexpr int Count = 6;     // The generator never creates more links than this

    inline Color GetColor(int link)
    {
        constexpr Color colors[Count] = {
            { 230, 60, 60, 255 },       // Red
            { 70, 130, 240, 255 },      // Blue
            { 240, 210, 60, 255 },      // Yellow
            { 70, 200, 90, 255 },       // Green
            { 170, 90, 230, 255 },      // Purple
            { 245, 140, 40, 255 },      // Orange
        };
        return colors[((link%Count) + Count)%Count];
    }

    inline const char *GetName(int link)
    {
        constexpr const char *names[Count] = { "RED", "BLUE", "YELLOW", "GREEN", "PURPLE", "ORANGE" };
        return names[((link%Count) + Count)%Count];
    }
}
