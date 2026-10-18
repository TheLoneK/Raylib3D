#pragma once

#include "raylib.h"
#include "ui/Gui.h"
#include <set>
#include <string>

struct EditorState;
struct LibraryNode;

//----------------------------------------------------------------------------------
// Left panel: folder tree of the asset library and details of the selected file.
// The selected folder is where imports go.
//----------------------------------------------------------------------------------
class LibraryPanel
{
public:
    LibraryPanel();
    void Draw(Rectangle bounds, EditorState &state);
    void Expand(const std::string &folder);     // Open a folder and its parents (after importing into it)

private:
    void DrawNode(const LibraryNode &node, int depth, Rectangle list, float &y, EditorState &state);
    void DrawDetails(Rectangle bounds, EditorState &state);
    std::string GetUsers(const EditorState &state, const std::string &path) const;     // Slots that use a file

    std::set<std::string> m_expanded;
    Gui::ScrollState m_scroll;
    float m_contentHeight = 0.0f;
};
