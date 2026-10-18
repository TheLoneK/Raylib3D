#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Editor look: colors and sizes in one place
//----------------------------------------------------------------------------------
namespace Theme
{
    inline constexpr int FontSize = 20;             // raylib's default font is crisp at multiples of 10
    inline constexpr int SmallFontSize = 10;
    inline constexpr float RowHeight = 26.0f;
    inline constexpr float Padding = 8.0f;
    inline constexpr float ToolbarHeight = 44.0f;
    inline constexpr float PanelTitleHeight = 30.0f;

    inline constexpr Color Background = { 24, 25, 30, 255 };
    inline constexpr Color Panel = { 34, 36, 43, 255 };
    inline constexpr Color PanelTitle = { 44, 47, 56, 255 };
    inline constexpr Color Border = { 60, 63, 75, 255 };
    inline constexpr Color Text = { 225, 228, 235, 255 };
    inline constexpr Color TextDim = { 135, 140, 155, 255 };
    inline constexpr Color Accent = { 255, 196, 70, 255 };
    inline constexpr Color Selection = { 70, 110, 180, 255 };
    inline constexpr Color Hover = { 52, 56, 68, 255 };
    inline constexpr Color Button = { 56, 60, 72, 255 };
    inline constexpr Color ButtonHover = { 72, 78, 94, 255 };
    inline constexpr Color ButtonPressed = { 90, 98, 120, 255 };
    inline constexpr Color Field = { 22, 23, 28, 255 };
    inline constexpr Color ModelTag = { 110, 190, 255, 255 };
    inline constexpr Color TextureTag = { 140, 220, 120, 255 };
    inline constexpr Color Warning = { 255, 120, 90, 255 };
    inline constexpr Color Good = { 120, 220, 140, 255 };
    inline constexpr Color Viewport = { 46, 48, 58, 255 };
}
