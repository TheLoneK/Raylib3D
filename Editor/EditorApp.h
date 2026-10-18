#pragma once

#include "raylib.h"
#include "assets/AssetLibrary.h"
#include "EditorState.h"
#include "panels/BindingsPanel.h"
#include "panels/ImportDialog.h"
#include "panels/LibraryPanel.h"
#include "panels/ViewportPanel.h"
#include <string>
#include <vector>

//----------------------------------------------------------------------------------
// The asset editor: toolbar on top, Library (left), Viewport (center), Bindings
// (right). Owns the panels and the actions that change files on disk.
// Create it after InitWindow (it uploads models/textures).
//----------------------------------------------------------------------------------
class EditorApp
{
public:
    EditorApp();
    ~EditorApp();
    EditorApp(const EditorApp &) = delete;
    EditorApp &operator=(const EditorApp &) = delete;

    void Frame();               // Update + draw one frame
    void SaveOnClose();

private:
    enum class Prompt
    {
        None,
        NewFolder,
        Delete
    };

    void HandleShortcuts();
    void HandleDroppedFiles();
    void Import(const std::vector<std::string> &paths, const std::string &folder);
    void Save();
    void Refresh();
    void DeleteSelected();
    void DrawToolbar(Rectangle bar);
    void DrawStatusBar(Rectangle bar);
    void DrawPrompt();

    AssetLibrary m_library;
    EditorState m_state;
    LibraryPanel m_libraryPanel;
    ViewportPanel m_viewportPanel;
    BindingsPanel m_bindingsPanel;
    ImportDialog m_importDialog;

    Prompt m_prompt = Prompt::None;
    std::string m_promptText;           // New folder name
};
