#pragma once

#include "raylib.h"
#include "library/LibraryTree.h"
#include "ui/Gui.h"
#include <set>
#include <string>
#include <vector>

//----------------------------------------------------------------------------------
// Modal file browser for picking models/textures (or a whole folder) on disk.
// Raylib has no native file dialog, so this is a small one of our own; files can
// also be dragged onto the editor window instead.
//----------------------------------------------------------------------------------
class ImportDialog
{
public:
    void Open(const std::string &destinationFolder);
    bool IsOpen() const { return m_open; }

    // Draw over everything. Returns true on the frame the user confirms; the picks are in GetPicked()
    bool Draw();
    const std::vector<std::string> &GetPicked() const { return m_picked; }

private:
    enum class Filter
    {
        Supported,
        Models,
        Textures,
        Everything
    };

    struct Entry
    {
        std::string name;
        std::string path;
        bool folder;
        LibraryItemKind kind;
    };

    void Navigate(const std::string &directory);
    bool Passes(const Entry &entry) const;

    bool m_open = false;
    std::string m_directory;
    std::string m_pathText;
    std::string m_destination;
    std::string m_error;
    std::vector<Entry> m_entries;
    std::vector<std::string> m_roots;               // Drive letters on Windows, "/" elsewhere
    std::set<std::string> m_selected;
    std::vector<std::string> m_picked;
    Filter m_filter = Filter::Supported;
    Gui::ScrollState m_scroll;
};
