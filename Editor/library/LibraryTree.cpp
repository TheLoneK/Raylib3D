#include "library/LibraryTree.h"
#include "assets/AssetManifest.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    std::string Lower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    void ScanFolder(const fs::path &folder, LibraryNode &node)
    {
        std::error_code error;
        for (const fs::directory_entry &entry : fs::directory_iterator(folder, error))
        {
            std::string name = entry.path().filename().string();
            if (name.empty() || (name[0] == '.')) continue;                    // .gitkeep and other hidden files
            if (node.path.empty() && (name == "library.ini")) continue;        // The bindings file itself

            LibraryNode child;
            child.name = name;
            if (entry.is_directory(error))
            {
                child.path = node.path + name + "/";
                child.kind = LibraryItemKind::Folder;
                ScanFolder(entry.path(), child);
            }
            else
            {
                child.path = node.path + name;
                child.kind = GetLibraryItemKind(name);
                child.size = entry.file_size(error);
            }
            node.children.push_back(std::move(child));
        }

        // Folders first, then by name
        std::sort(node.children.begin(), node.children.end(), [](const LibraryNode &a, const LibraryNode &b) {
            if (a.IsFolder() != b.IsFolder()) return a.IsFolder();
            return Lower(a.name) < Lower(b.name);
        });
    }

    const LibraryNode *FindIn(const LibraryNode &node, const std::string &path)
    {
        if (node.path == path) return &node;
        for (const LibraryNode &child : node.children)
        {
            // Only descend into folders that contain the path
            if (child.IsFolder() && (path.compare(0, child.path.size(), child.path) != 0)) continue;
            const LibraryNode *found = FindIn(child, path);
            if (found != nullptr) return found;
        }
        return nullptr;
    }
}

LibraryItemKind GetLibraryItemKind(const std::string &path)
{
    if (!path.empty() && (path.back() == '/')) return LibraryItemKind::Folder;
    if (AssetFiles::IsModel(path)) return LibraryItemKind::Model;
    if (AssetFiles::IsTexture(path)) return LibraryItemKind::Texture;
    return LibraryItemKind::Other;
}

std::string GetParentFolder(const std::string &libraryPath)
{
    std::string path = libraryPath;
    if (!path.empty() && (path.back() == '/')) path.pop_back();
    size_t slash = path.find_last_of('/');
    return (slash == std::string::npos) ? "" : path.substr(0, slash + 1);
}

void LibraryTree::Scan()
{
    std::string root = AssetFiles::GetLibraryRoot();
    std::error_code error;
    fs::create_directories(fs::path(root) / "Models", error);
    fs::create_directories(fs::path(root) / "Textures", error);

    m_root = LibraryNode{};
    m_root.name = "Assets";
    ScanFolder(fs::path(root), m_root);
}

const LibraryNode *LibraryTree::Find(const std::string &path) const
{
    return FindIn(m_root, path);
}

int LibraryTree::CountFiles(const LibraryNode &node) const
{
    int count = 0;
    for (const LibraryNode &child : node.children) count += child.IsFolder() ? CountFiles(child) : 1;
    return count;
}
