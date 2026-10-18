#include "ui/Gui.h"
#include "ui/Theme.h"
#include "raymath.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    constexpr float DoubleClickTime = 0.35f;
    constexpr float ScrollbarWidth = 8.0f;

    struct ScrollFrame
    {
        Rectangle view;
        Gui::ScrollState *state;
        float contentHeight;
    };

    bool s_inputEnabled = true;
    std::vector<Rectangle> s_clips;
    std::vector<ScrollFrame> s_scrolls;
    Vector2 s_pressPosition[3] = {};
    bool s_doubleClick = false;
    double s_lastClickTime = -10.0;
    Vector2 s_lastClickPosition = {};

    const void *s_activeValue = nullptr;            // Slider / drag field being dragged
    float s_dragStartValue = 0.0f;
    float s_dragStartX = 0.0f;
    const void *s_scrollbarDrag = nullptr;
    float s_scrollbarGrab = 0.0f;

    std::string *s_focusedText = nullptr;           // Text box with keyboard focus
    const void *s_editingValue = nullptr;           // Number field typed into after a double click
    std::string s_editBuffer;
    bool s_focusUsedThisFrame = false;
    int s_cursor = MOUSE_CURSOR_DEFAULT;            // Requested this frame by the widget under the mouse
    int s_appliedCursor = MOUSE_CURSOR_DEFAULT;

    int Size(int fontSize) { return (fontSize > 0) ? fontSize : Theme::FontSize; }

    Rectangle CurrentClip()
    {
        if (s_clips.empty()) return Rectangle{ 0.0f, 0.0f, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) };
        return s_clips.back();
    }

    Rectangle Intersect(Rectangle a, Rectangle b)
    {
        float x0 = std::max(a.x, b.x);
        float y0 = std::max(a.y, b.y);
        float x1 = std::min(a.x + a.width, b.x + b.width);
        float y1 = std::min(a.y + a.height, b.y + b.height);
        return Rectangle{ x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0) };
    }

    void ApplyClip()
    {
        if (s_clips.empty())
        {
            EndScissorMode();
            return;
        }
        Rectangle clip = s_clips.back();
        BeginScissorMode(static_cast<int>(clip.x), static_cast<int>(clip.y), static_cast<int>(clip.width), static_cast<int>(clip.height));
    }

    Color ButtonColor(Rectangle rect, bool enabled)
    {
        if (!enabled) return Fade(Theme::Button, 0.5f);
        if (Gui::IsHovered(rect)) return IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? Theme::ButtonPressed : Theme::ButtonHover;
        return Theme::Button;
    }

    void DrawTextCentered(Rectangle rect, const std::string &text, Color color)
    {
        std::string fitted = Gui::Ellipsize(text, rect.width - 8.0f);
        float width = static_cast<float>(Gui::MeasureWidth(fitted));
        Gui::Label(rect.x + (rect.width - width)*0.5f, rect.y + (rect.height - Theme::FontSize)*0.5f, fitted, color);
    }

    // Typing a number into a slider/drag field (double click). Returns true when it was committed
    bool EditNumber(Rectangle field, float &value, float min, float max, bool clampValue)
    {
        if (s_editingValue != &value) return false;
        s_focusUsedThisFrame = true;

        int character = GetCharPressed();
        while (character > 0)
        {
            if (((character >= '0') && (character <= '9')) || (character == '.') || (character == '-')) s_editBuffer += static_cast<char>(character);
            character = GetCharPressed();
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !s_editBuffer.empty()) s_editBuffer.pop_back();

        DrawRectangleRec(field, Theme::Field);
        DrawRectangleLinesEx(field, 1.0f, Theme::Accent);
        bool blink = fmod(GetTime(), 1.0) < 0.5;
        Gui::Label(field.x + 6.0f, field.y + (field.height - Theme::FontSize)*0.5f, s_editBuffer + (blink ? "_" : ""), Theme::Text);

        bool clickedAway = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), field);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || clickedAway)
        {
            s_editingValue = nullptr;
            try
            {
                float typed = std::stof(s_editBuffer);
                value = clampValue ? Clamp(typed, min, max) : typed;
                return true;
            }
            catch (const std::exception &) { return false; }
        }
        if (IsKeyPressed(KEY_ESCAPE)) s_editingValue = nullptr;
        return false;
    }

    void StartEditing(const void *value, float current, const char *format)
    {
        char text[64];
        std::snprintf(text, sizeof(text), format, current);
        s_editingValue = value;
        s_editBuffer = text;
        s_activeValue = nullptr;
        s_focusedText = nullptr;
        s_focusUsedThisFrame = true;    // Drawn as a text field from the next frame on
    }
}

namespace Gui
{
    //------------------------------------------------------------------------------
    // Frame / input
    //------------------------------------------------------------------------------
    void BeginFrame()
    {
        for (int button = 0; button < 3; button++)
        {
            if (IsMouseButtonPressed(button)) s_pressPosition[button] = GetMousePosition();
        }

        s_doubleClick = false;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            double now = GetTime();
            Vector2 mouse = GetMousePosition();
            s_doubleClick = ((now - s_lastClickTime) < DoubleClickTime) && (Vector2Distance(mouse, s_lastClickPosition) < 6.0f);
            s_lastClickTime = s_doubleClick ? -10.0 : now;
            s_lastClickPosition = mouse;
        }

        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            s_activeValue = nullptr;
            s_scrollbarDrag = nullptr;
        }
        s_focusUsedThisFrame = false;
        s_inputEnabled = true;
        s_cursor = MOUSE_CURSOR_DEFAULT;
    }

    void EndFrame()
    {
        // A text box or number edit that was not drawn this frame loses focus
        if (!s_focusUsedThisFrame)
        {
            s_focusedText = nullptr;
            s_editingValue = nullptr;
        }
        s_clips.clear();
        s_scrolls.clear();
        EndScissorMode();

        if (s_cursor != s_appliedCursor)
        {
            SetMouseCursor(s_cursor);
            s_appliedCursor = s_cursor;
        }
    }

    void SetInputEnabled(bool enabled) { s_inputEnabled = enabled; }
    bool IsInputEnabled() { return s_inputEnabled; }
    bool IsTyping() { return (s_focusedText != nullptr) || (s_editingValue != nullptr); }

    bool IsHovered(Rectangle rect)
    {
        if (!s_inputEnabled) return false;
        Vector2 mouse = GetMousePosition();
        return CheckCollisionPointRec(mouse, rect) && CheckCollisionPointRec(mouse, CurrentClip());
    }

    bool IsClicked(Rectangle rect, int button)
    {
        if (!IsMouseButtonReleased(button) || !IsHovered(rect)) return false;
        if ((button == MOUSE_BUTTON_LEFT) && (s_scrollbarDrag != nullptr)) return false;
        return CheckCollisionPointRec(s_pressPosition[button], rect);
    }

    bool IsDoubleClicked(Rectangle rect)
    {
        return s_doubleClick && IsClicked(rect);
    }

    bool IsPressedInside(Rectangle rect, int button)
    {
        return IsMouseButtonPressed(button) && IsHovered(rect);
    }

    void PushClip(Rectangle rect)
    {
        s_clips.push_back(Intersect(rect, CurrentClip()));
        ApplyClip();
    }

    void PopClip()
    {
        if (!s_clips.empty()) s_clips.pop_back();
        ApplyClip();
    }

    //------------------------------------------------------------------------------
    // Text
    //------------------------------------------------------------------------------
    void Label(float x, float y, const std::string &text, Color color, int fontSize)
    {
        int size = Size(fontSize);
        DrawTextEx(GetFontDefault(), text.c_str(), Vector2{ floorf(x), floorf(y) }, static_cast<float>(size), static_cast<float>(size/10), color);
    }

    void LabelFitted(Rectangle rect, const std::string &text, Color color, int fontSize)
    {
        int size = Size(fontSize);
        Label(rect.x, rect.y + (rect.height - static_cast<float>(size))*0.5f, Ellipsize(text, rect.width, fontSize), color, fontSize);
    }

    int MeasureWidth(const std::string &text, int fontSize)
    {
        return MeasureText(text.c_str(), Size(fontSize));
    }

    std::string Ellipsize(const std::string &text, float width, int fontSize)
    {
        if (static_cast<float>(MeasureWidth(text, fontSize)) <= width) return text;
        std::string result = text;
        while (!result.empty() && (static_cast<float>(MeasureWidth(result + "...", fontSize)) > width)) result.pop_back();
        return result + "...";
    }

    //------------------------------------------------------------------------------
    // Containers
    //------------------------------------------------------------------------------
    Rectangle Panel(Rectangle rect, const char *title)
    {
        DrawRectangleRec(rect, Theme::Panel);
        Rectangle titleBar = { rect.x, rect.y, rect.width, Theme::PanelTitleHeight };
        DrawRectangleRec(titleBar, Theme::PanelTitle);
        Label(rect.x + Theme::Padding, rect.y + (Theme::PanelTitleHeight - Theme::FontSize)*0.5f, title, Theme::Accent);
        DrawRectangleLinesEx(rect, 1.0f, Theme::Border);
        return Rectangle{ rect.x + 1.0f, rect.y + Theme::PanelTitleHeight, rect.width - 2.0f, rect.height - Theme::PanelTitleHeight - 1.0f };
    }

    void Separator(float x, float y, float width)
    {
        DrawLineEx(Vector2{ x, y }, Vector2{ x + width, y }, 1.0f, Theme::Border);
    }

    //------------------------------------------------------------------------------
    // Widgets
    //------------------------------------------------------------------------------
    bool Button(Rectangle rect, const char *label, bool enabled, bool highlighted)
    {
        DrawRectangleRec(rect, highlighted && enabled ? ColorLerp(ButtonColor(rect, enabled), Theme::Accent, 0.35f) : ButtonColor(rect, enabled));
        DrawRectangleLinesEx(rect, 1.0f, highlighted && enabled ? Theme::Accent : Theme::Border);
        DrawTextCentered(rect, label, enabled ? Theme::Text : Theme::TextDim);
        return enabled && IsClicked(rect);
    }

    bool Checkbox(Rectangle rect, const char *label, bool &value)
    {
        float box = 18.0f;
        Rectangle square = { rect.x, rect.y + (rect.height - box)*0.5f, box, box };
        if (IsHovered(rect)) DrawRectangleRec(rect, Theme::Hover);
        DrawRectangleRec(square, Theme::Field);
        DrawRectangleLinesEx(square, 1.0f, Theme::Border);
        if (value) DrawRectangleRec(Rectangle{ square.x + 4.0f, square.y + 4.0f, box - 8.0f, box - 8.0f }, Theme::Accent);
        LabelFitted(Rectangle{ rect.x + box + 8.0f, rect.y, rect.width - box - 8.0f, rect.height }, label, Theme::Text);

        if (!IsClicked(rect)) return false;
        value = !value;
        return true;
    }

    bool Toggle(Rectangle rect, const char *label, bool &value)
    {
        if (!Button(rect, label, true, value)) return false;
        value = !value;
        return true;
    }

    bool SelectableRow(Rectangle rect, bool selected)
    {
        if (selected) DrawRectangleRec(rect, Theme::Selection);
        else if (IsHovered(rect)) DrawRectangleRec(rect, Theme::Hover);
        return IsClicked(rect);
    }

    bool Slider(Rectangle rect, const char *label, float &value, float min, float max, const char *format)
    {
        float labelWidth = std::min(rect.width*0.38f, 120.0f);
        LabelFitted(Rectangle{ rect.x, rect.y, labelWidth - 4.0f, rect.height }, label, Theme::TextDim);
        Rectangle track = { rect.x + labelWidth, rect.y + 2.0f, rect.width - labelWidth, rect.height - 4.0f };

        if (EditNumber(track, value, min, max, true)) return true;
        if (IsDoubleClicked(track))
        {
            StartEditing(&value, value, format);
            return false;
        }

        bool changed = false;
        if (IsPressedInside(track)) s_activeValue = &value;
        if ((s_activeValue == &value) && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            float t = Clamp((GetMouseX() - track.x)/track.width, 0.0f, 1.0f);
            float next = Lerp(min, max, t);
            changed = (next != value);
            value = next;
        }

        float fill = Clamp((value - min)/(max - min), 0.0f, 1.0f);
        DrawRectangleRec(track, Theme::Field);
        DrawRectangleRec(Rectangle{ track.x, track.y, track.width*fill, track.height }, Fade(Theme::Selection, IsHovered(track) || (s_activeValue == &value) ? 0.9f : 0.7f));
        DrawRectangleLinesEx(track, 1.0f, Theme::Border);
        DrawTextCentered(track, TextFormat(format, value), Theme::Text);
        return changed;
    }

    bool DragFloat(Rectangle rect, const char *label, float &value, float speed, const char *format)
    {
        float labelWidth = std::min(rect.width*0.38f, 120.0f);
        LabelFitted(Rectangle{ rect.x, rect.y, labelWidth - 4.0f, rect.height }, label, Theme::TextDim);
        Rectangle field = { rect.x + labelWidth, rect.y + 2.0f, rect.width - labelWidth, rect.height - 4.0f };

        if (EditNumber(field, value, 0.0f, 0.0f, false)) return true;
        if (IsDoubleClicked(field))
        {
            StartEditing(&value, value, format);
            return false;
        }

        bool changed = false;
        if (IsPressedInside(field))
        {
            s_activeValue = &value;
            s_dragStartValue = value;
            s_dragStartX = static_cast<float>(GetMouseX());
        }
        if ((s_activeValue == &value) && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            float scale = IsKeyDown(KEY_LEFT_SHIFT) ? 0.1f : 1.0f;      // Shift for fine control
            float next = s_dragStartValue + (GetMouseX() - s_dragStartX)*speed*scale;
            changed = (next != value);
            value = next;
        }

        bool active = (s_activeValue == &value);
        DrawRectangleRec(field, active ? Theme::ButtonPressed : (IsHovered(field) ? Theme::ButtonHover : Theme::Field));
        DrawRectangleLinesEx(field, 1.0f, Theme::Border);
        DrawTextCentered(field, TextFormat(format, value), Theme::Text);
        if (IsHovered(field) || active) s_cursor = MOUSE_CURSOR_RESIZE_EW;
        return changed;
    }

    void Focus(std::string &text)
    {
        s_focusedText = &text;
        s_editingValue = nullptr;
        s_focusUsedThisFrame = true;
    }

    bool TextBox(Rectangle rect, std::string &text, bool &submitted)
    {
        submitted = false;
        if (IsClicked(rect))
        {
            s_focusedText = &text;
            s_editingValue = nullptr;
        }
        bool focused = (s_focusedText == &text);
        bool changed = false;

        if (focused)
        {
            s_focusUsedThisFrame = true;
            int character = GetCharPressed();
            while (character > 0)
            {
                if ((character >= 32) && (character < 127))
                {
                    text += static_cast<char>(character);
                    changed = true;
                }
                character = GetCharPressed();
            }
            if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty())
            {
                text.pop_back();
                changed = true;
            }
            bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            if (control && IsKeyPressed(KEY_V))
            {
                const char *clipboard = GetClipboardText();
                if (clipboard != nullptr)
                {
                    text += clipboard;
                    changed = true;
                }
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
            {
                submitted = true;
                s_focusedText = nullptr;
            }
            if (IsKeyPressed(KEY_ESCAPE) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !IsHovered(rect))) s_focusedText = nullptr;
        }

        DrawRectangleRec(rect, Theme::Field);
        DrawRectangleLinesEx(rect, 1.0f, focused ? Theme::Accent : Theme::Border);

        // Show the end of long text (where the cursor is)
        std::string shown = text;
        float room = rect.width - 16.0f;
        while ((shown.size() > 1) && (static_cast<float>(MeasureWidth(shown + "_")) > room)) shown.erase(0, 1);
        bool blink = focused && (fmod(GetTime(), 1.0) < 0.5);
        Label(rect.x + 6.0f, rect.y + (rect.height - Theme::FontSize)*0.5f, shown + (blink ? "_" : ""), Theme::Text);
        if (IsHovered(rect)) s_cursor = MOUSE_CURSOR_IBEAM;
        return changed;
    }

    //------------------------------------------------------------------------------
    // Scrolling
    //------------------------------------------------------------------------------
    void BeginScroll(Rectangle view, ScrollState &state, float contentHeight)
    {
        float maxOffset = std::max(0.0f, contentHeight - view.height);
        if (IsHovered(view)) state.offset -= GetMouseWheelMove()*Theme::RowHeight*3.0f;

        // Dragging the scrollbar thumb
        if ((s_scrollbarDrag == &state) && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && (maxOffset > 0.0f))
        {
            float thumbHeight = std::max(24.0f, view.height*view.height/contentHeight);
            float t = (GetMouseY() - s_scrollbarGrab - view.y)/(view.height - thumbHeight);
            state.offset = t*maxOffset;
        }
        state.offset = Clamp(state.offset, 0.0f, maxOffset);

        Rectangle bar = { view.x + view.width - ScrollbarWidth, view.y, ScrollbarWidth, view.height };
        if ((maxOffset > 0.0f) && IsPressedInside(bar))
        {
            float thumbHeight = std::max(24.0f, view.height*view.height/contentHeight);
            float thumbY = view.y + (view.height - thumbHeight)*(state.offset/maxOffset);
            s_scrollbarDrag = &state;
            s_scrollbarGrab = ((GetMouseY() >= thumbY) && (GetMouseY() <= thumbY + thumbHeight)) ? GetMouseY() - thumbY : thumbHeight*0.5f;
        }

        s_scrolls.push_back(ScrollFrame{ view, &state, contentHeight });
        PushClip(view);
    }

    void EndScroll()
    {
        PopClip();
        if (s_scrolls.empty()) return;
        ScrollFrame frame = s_scrolls.back();
        s_scrolls.pop_back();

        float maxOffset = frame.contentHeight - frame.view.height;
        if (maxOffset <= 0.0f) return;
        float thumbHeight = std::max(24.0f, frame.view.height*frame.view.height/frame.contentHeight);
        float thumbY = frame.view.y + (frame.view.height - thumbHeight)*(frame.state->offset/maxOffset);
        Rectangle bar = { frame.view.x + frame.view.width - ScrollbarWidth, frame.view.y, ScrollbarWidth, frame.view.height };
        DrawRectangleRec(bar, Fade(BLACK, 0.25f));
        DrawRectangleRounded(Rectangle{ bar.x + 1.0f, thumbY, ScrollbarWidth - 2.0f, thumbHeight }, 0.5f, 4, (s_scrollbarDrag == frame.state) ? Theme::Accent : Theme::ButtonHover);
    }
}
