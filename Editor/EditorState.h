#pragma once

#include "raylib.h"
#include "assets/AssetLibrary.h"
#include "assets/AssetManifest.h"
#include "library/LibraryTree.h"
#include <string>

//----------------------------------------------------------------------------------
// What the editor panels share: the library on disk, the bindings being edited,
// the selection, and the status line
//----------------------------------------------------------------------------------
enum class PreviewSource
{
    None,
    File,       // A model/texture picked in the library panel
    Slot        // An enemy/weapon picked in the bindings panel (shows its bound model as the game will)
};

struct EditorState
{
    explicit EditorState(AssetLibrary &assetLibrary) : library(assetLibrary) {}

    AssetLibrary &library;
    AssetManifest manifest;             // Edited copy of library.ini (saved with Save / Ctrl+S)
    LibraryTree tree;
    std::string selectedPath;           // Library path of the selected file or folder ("" = root)
    std::string selectedSlot;           // AssetSlot::id
    PreviewSource preview = PreviewSource::None;
    bool dirty = false;

    std::string status = "Drop files onto the window or press Import to add models and textures";
    Color statusColor = { 135, 140, 155, 255 };
    double statusTime = 0.0;

    void SetStatus(const std::string &text, Color color)
    {
        status = text;
        statusColor = color;
        statusTime = GetTime();
    }

    // Call after editing manifest: the preview picks up the change at once
    void MarkChanged()
    {
        dirty = true;
        library.SetManifest(manifest);
    }

    // Where imports go: the selected folder, or the folder of the selected file ("" = sort by type)
    std::string GetImportFolder() const
    {
        if (selectedPath.empty() || (selectedPath.back() == '/')) return selectedPath;
        return GetParentFolder(selectedPath);
    }

    bool IsSelectedFileKind(LibraryItemKind kind) const
    {
        return !selectedPath.empty() && (GetLibraryItemKind(selectedPath) == kind);
    }
};
