#include "panels/LibraryPanel.h"
#include "EditorState.h"
#include "assets/AssetSlots.h"
#include "library/LibraryTree.h"
#include "ui/Theme.h"
#include <string>

namespace
{
    constexpr float DetailsHeight = 150.0f;
    constexpr float Indent = 18.0f;

    const char *Tag(LibraryItemKind kind)
    {
        switch (kind)
        {
            case LibraryItemKind::Model: return "M";
            case LibraryItemKind::Texture: return "T";
            default: return "-";
        }
    }

    Color TagColor(LibraryItemKind kind)
    {
        switch (kind)
        {
            case LibraryItemKind::Model: return Theme::ModelTag;
            case LibraryItemKind::Texture: return Theme::TextureTag;
            default: return Theme::TextDim;
        }
    }

    std::string FormatSize(std::uintmax_t bytes)
    {
        if (bytes < 1024) return TextFormat("%d B", static_cast<int>(bytes));
        if (bytes < 1024*1024) return TextFormat("%.1f KB", static_cast<double>(bytes)/1024.0);
        return TextFormat("%.1f MB", static_cast<double>(bytes)/(1024.0*1024.0));
    }

    void DrawFolderIcon(float x, float y, Color color)
    {
        DrawRectangleRec(Rectangle{ x, y + 2.0f, 7.0f, 3.0f }, color);
        DrawRectangleRec(Rectangle{ x, y + 4.0f, 16.0f, 11.0f }, color);
    }

    void DrawArrow(float x, float y, bool open, Color color)
    {
        if (open) DrawTriangle(Vector2{ x, y }, Vector2{ x + 5.0f, y + 6.0f }, Vector2{ x + 10.0f, y }, color);
        else DrawTriangle(Vector2{ x + 2.0f, y - 3.0f }, Vector2{ x + 2.0f, y + 9.0f }, Vector2{ x + 8.0f, y + 3.0f }, color);
    }
}

LibraryPanel::LibraryPanel()
{
    m_expanded = { "", "Models/", "Textures/" };
}

void LibraryPanel::Expand(const std::string &folder)
{
    std::string path = folder;
    while (!path.empty())
    {
        m_expanded.insert(path);
        path = GetParentFolder(path);
    }
    m_expanded.insert("");
}

std::string LibraryPanel::GetUsers(const EditorState &state, const std::string &path) const
{
    std::string users;
    for (const AssetSlot &slot : GetAssetSlots())
    {
        const AssetBinding *binding = state.manifest.Find(slot.id);
        if ((binding == nullptr) || ((binding->model != path) && (binding->texture != path))) continue;
        if (!users.empty()) users += ", ";
        users += slot.label;
    }
    return users;
}

void LibraryPanel::DrawNode(const LibraryNode &node, int depth, Rectangle list, float &y, EditorState &state)
{
    Rectangle row = { list.x, list.y + y - m_scroll.offset, list.width - 10.0f, Theme::RowHeight };
    y += Theme::RowHeight;
    bool visible = (row.y + row.height >= list.y) && (row.y <= list.y + list.height);
    bool open = node.IsFolder() && m_expanded.contains(node.path);

    if (visible)
    {
        bool selected = (state.selectedPath == node.path);
        if (Gui::SelectableRow(row, selected))
        {
            state.selectedPath = node.path;
            if (!node.IsFolder() && (node.kind != LibraryItemKind::Other)) state.preview = PreviewSource::File;

            // Folders open/close on the arrow or with a double click
            bool onArrow = GetMouseX() < row.x + 8.0f + static_cast<float>(depth)*Indent + 14.0f;
            if (node.IsFolder() && (onArrow || Gui::IsDoubleClicked(row)))
            {
                if (open) m_expanded.erase(node.path);
                else m_expanded.insert(node.path);
            }
        }

        float x = row.x + 8.0f + static_cast<float>(depth)*Indent;
        float middle = row.y + row.height*0.5f;
        if (node.IsFolder())
        {
            if (!node.children.empty()) DrawArrow(x, middle - 3.0f, open, Theme::TextDim);
            DrawFolderIcon(x + 14.0f, middle - 8.0f, Theme::Accent);
            Gui::LabelFitted(Rectangle{ x + 38.0f, row.y, row.width - (x + 38.0f - row.x), row.height }, node.path.empty() ? "Assets" : node.name, Theme::Text);
        }
        else
        {
            Color tag = TagColor(node.kind);
            Gui::Label(x + 18.0f, middle - Theme::FontSize*0.5f, Tag(node.kind), tag);
            bool used = !GetUsers(state, node.path).empty();
            float nameWidth = row.width - (x + 38.0f - row.x) - (used ? 16.0f : 0.0f);
            Gui::LabelFitted(Rectangle{ x + 38.0f, row.y, nameWidth, row.height }, node.name, (node.kind == LibraryItemKind::Other) ? Theme::TextDim : Theme::Text);
            if (used) DrawCircleV(Vector2{ row.x + row.width - 10.0f, middle }, 4.0f, Theme::Accent);     // Bound to a slot
        }
    }

    if (open)
    {
        for (const LibraryNode &child : node.children) DrawNode(child, depth + 1, list, y, state);
    }
}

void LibraryPanel::DrawDetails(Rectangle bounds, EditorState &state)
{
    DrawRectangleRec(bounds, Theme::PanelTitle);
    Gui::Separator(bounds.x, bounds.y, bounds.width);
    float x = bounds.x + Theme::Padding;
    float y = bounds.y + Theme::Padding;
    float width = bounds.width - Theme::Padding*2.0f;
    auto line = [&](const std::string &text, Color color) {
        Gui::LabelFitted(Rectangle{ x, y, width, 22.0f }, text, color);
        y += 22.0f;
    };

    const LibraryNode *node = state.tree.Find(state.selectedPath);
    if (node == nullptr)
    {
        line("Nothing selected", Theme::TextDim);
        return;
    }

    line(node->path.empty() ? "Assets/" : node->path, Theme::Text);
    if (node->IsFolder())
    {
        line(TextFormat("Folder, %d files", state.tree.CountFiles(*node)), Theme::TextDim);
        line("Imports go into this folder", Theme::TextDim);
        if (node->path.empty()) line("(root: sorted into Models/ and Textures/)", Theme::TextDim);
        return;
    }

    switch (node->kind)
    {
        case LibraryItemKind::Model:
        {
            const LoadedModel &model = state.library.GetModel(node->path);
            if (!model.IsValid())
            {
                line("Model: " + model.error, Theme::Warning);
                break;
            }
            int triangles = 0;
            for (int i = 0; i < model.model.meshCount; i++) triangles += model.model.meshes[i].triangleCount;
            line(TextFormat("Model, %s, %d triangles", FormatSize(node->size).c_str(), triangles), Theme::TextDim);
            line(TextFormat("%d meshes, %d materials, %d animations", model.model.meshCount, model.model.materialCount, model.animationCount), Theme::TextDim);
        } break;
        case LibraryItemKind::Texture:
        {
            Texture2D texture = state.library.GetTexture(node->path);
            if (texture.id == 0) line("Texture: could not load (format not enabled in raylib?)", Theme::Warning);
            else line(TextFormat("Texture, %s, %d x %d", FormatSize(node->size).c_str(), texture.width, texture.height), Theme::TextDim);
        } break;
        default:
        {
            line(TextFormat("Companion file, %s", FormatSize(node->size).c_str()), Theme::TextDim);
        } break;
    }

    std::string users = GetUsers(state, node->path);
    if (!users.empty()) line("Used by: " + users, Theme::Accent);
}

void LibraryPanel::Draw(Rectangle bounds, EditorState &state)
{
    Rectangle content = Gui::Panel(bounds, "Library");
    Rectangle list = { content.x, content.y + 4.0f, content.width, content.height - DetailsHeight - 4.0f };

    Gui::BeginScroll(list, m_scroll, m_contentHeight);
        float y = 0.0f;
        DrawNode(state.tree.GetRoot(), 0, list, y, state);
        m_contentHeight = y + Theme::RowHeight;
    Gui::EndScroll();

    DrawDetails(Rectangle{ content.x, content.y + content.height - DetailsHeight, content.width, DetailsHeight }, state);
}
