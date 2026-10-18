#pragma once

#include "raylib.h"
#include "ui/Gui.h"

struct EditorState;
struct AssetSlot;

//----------------------------------------------------------------------------------
// Right panel: every enemy and weapon that can use an imported model, and the
// settings of the selected one (model, texture, size, offset, rotation, animation)
//----------------------------------------------------------------------------------
class BindingsPanel
{
public:
    void Draw(Rectangle bounds, EditorState &state);

private:
    float DrawSlotList(Rectangle area, EditorState &state);         // Returns the height used
    void DrawInspector(Rectangle area, EditorState &state, const AssetSlot &slot);

    Gui::ScrollState m_listScroll;
    Gui::ScrollState m_inspectorScroll;
    float m_inspectorHeight = 0.0f;
};
