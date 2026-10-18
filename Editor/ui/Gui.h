#pragma once

#include "raylib.h"
#include <string>

//----------------------------------------------------------------------------------
// Tiny immediate mode GUI for the editor (raylib only, no extra libraries).
// Widgets are plain function calls each frame; they draw themselves and return
// whether they were used. Values being dragged/edited are identified by address.
//----------------------------------------------------------------------------------
namespace Gui
{
    void BeginFrame();
    void EndFrame();

    // Modal dialogs: while blocked, widgets outside the modal ignore the mouse/keyboard
    void SetInputEnabled(bool enabled);
    bool IsInputEnabled();
    bool IsTyping();                                // A text box has focus (skip keyboard shortcuts)

    // Mouse helpers that respect clipping and modals
    bool IsHovered(Rectangle rect);
    bool IsClicked(Rectangle rect, int button = MOUSE_BUTTON_LEFT);         // Pressed and released inside
    bool IsDoubleClicked(Rectangle rect);
    bool IsPressedInside(Rectangle rect, int button = MOUSE_BUTTON_LEFT);   // Press started inside this frame

    // Clipping for scroll areas (nests)
    void PushClip(Rectangle rect);
    void PopClip();

    // Text
    void Label(float x, float y, const std::string &text, Color color, int fontSize = 0);
    void LabelFitted(Rectangle rect, const std::string &text, Color color, int fontSize = 0);  // Ellipsized to the width, vertically centered
    int MeasureWidth(const std::string &text, int fontSize = 0);
    std::string Ellipsize(const std::string &text, float width, int fontSize = 0);

    // Containers
    Rectangle Panel(Rectangle rect, const char *title);      // Returns the content area
    void Separator(float x, float y, float width);

    // Widgets
    bool Button(Rectangle rect, const char *label, bool enabled = true, bool highlighted = false);
    bool Checkbox(Rectangle rect, const char *label, bool &value);
    bool Toggle(Rectangle rect, const char *label, bool &value);              // Button that stays pressed
    bool SelectableRow(Rectangle rect, bool selected);                        // Background only, draw the content yourself
    bool Slider(Rectangle rect, const char *label, float &value, float min, float max, const char *format = "%.2f");
    bool DragFloat(Rectangle rect, const char *label, float &value, float speed, const char *format = "%.3f");   // Drag left/right, unbounded
    bool TextBox(Rectangle rect, std::string &text, bool &submitted);         // Returns true when the text changed
    void Focus(std::string &text);                                            // Give a text box keyboard focus (draw it this frame)

    // Scrolling list: call Begin, draw rows at y = content.y + i*rowHeight - offset, then End
    struct ScrollState
    {
        float offset = 0.0f;
    };
    void BeginScroll(Rectangle view, ScrollState &state, float contentHeight);
    void EndScroll();
}
