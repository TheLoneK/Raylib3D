#pragma once

#include <cstdint>
#include <string>
#include <vector>

//----------------------------------------------------------------------------------
// Snapshot of the folders and files under the asset library root (Assets/)
//----------------------------------------------------------------------------------
enum class LibraryItemKind
{
    Folder,
    Model,
    Texture,
    Other       // Companion files (.mtl, .bin...) and anything else
};

struct LibraryNode
{
    std::string name;
    std::string path;               // Library path: "" for the root, folders end with '/'
    LibraryItemKind kind = LibraryItemKind::Folder;
    std::uintmax_t size = 0;        // Bytes (files)
    std::vector<LibraryNode> children;

    bool IsFolder() const { return kind == LibraryItemKind::Folder; }
};

class LibraryTree
{
public:
    void Scan();                    // Re-read the folder from disk
    const LibraryNode &GetRoot() const { return m_root; }
    const LibraryNode *Find(const std::string &path) const;
    int CountFiles(const LibraryNode &node) const;

private:
    LibraryNode m_root;
};

LibraryItemKind GetLibraryItemKind(const std::string &path);
std::string GetParentFolder(const std::string &libraryPath);     // "Models/a.glb" -> "Models/", "Models/" -> ""
