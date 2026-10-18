#include "EditorApp.h"
#include "assets/AssetSlots.h"
#include "library/Importer.h"
#include "ui/Gui.h"
#include "ui/Theme.h"
#include <algorithm>

namespace
{
    constexpr float LibraryWidth = 320.0f;
    constexpr float BindingsWidth = 380.0f;
    constexpr float StatusHeight = 26.0f;
}

EditorApp::EditorApp() : m_state(m_library)
{
    m_state.tree.Scan();
    m_state.manifest.Load(AssetFiles::GetManifestPath());
    m_library.SetManifest(m_state.manifest);
    m_state.selectedSlot = GetAssetSlots().front().id;
}

EditorApp::~EditorApp() = default;

//----------------------------------------------------------------------------------
// Actions
//----------------------------------------------------------------------------------
void EditorApp::Save()
{
    if (m_state.manifest.Save(AssetFiles::GetManifestPath()))
    {
        m_state.dirty = false;
        m_state.SetStatus("Saved Assets/library.ini. Press F8 in the game to reload it (or restart the game)", Theme::Good);
    }
    else m_state.SetStatus("Could not write " + AssetFiles::GetManifestPath(), Theme::Warning);
}

void EditorApp::Refresh()
{
    m_state.tree.Scan();
    if ((m_state.tree.Find(m_state.selectedPath) == nullptr)) m_state.selectedPath.clear();
}

void EditorApp::Import(const std::vector<std::string> &paths, const std::string &folder)
{
    ImportResult result = Importer::Import(paths, folder);
    for (const std::string &path : result.imported) m_library.UnloadFile(path);       // Re-imports replace the cached copy
    Refresh();

    if (!result.imported.empty())
    {
        m_state.selectedPath = result.imported.front();
        m_state.preview = PreviewSource::File;
        m_libraryPanel.Expand(GetParentFolder(result.imported.front()));
    }

    std::string text = TextFormat("Imported %d asset%s (%d files copied)", static_cast<int>(result.imported.size()), (result.imported.size() == 1) ? "" : "s", result.copiedFiles);
    if (!result.messages.empty()) text += ". " + result.messages.front() + ((result.messages.size() > 1) ? TextFormat(" (+%d more, see the log)", static_cast<int>(result.messages.size()) - 1) : "");
    for (const std::string &message : result.messages) TraceLog(LOG_WARNING, "IMPORT: %s", message.c_str());
    m_state.SetStatus(text, (result.failed > 0) || result.imported.empty() ? Theme::Warning : Theme::Good);
}

void EditorApp::DeleteSelected()
{
    std::string path = m_state.selectedPath;
    std::string error;
    if (!Importer::Delete(path, error))
    {
        m_state.SetStatus("Could not delete " + path + ": " + error, Theme::Warning);
        return;
    }

    // Bindings that used the deleted file(s) go back to the built-in look
    int cleared = 0;
    bool folder = !path.empty() && (path.back() == '/');
    auto matches = [&](const std::string &used) { return !used.empty() && (folder ? (used.rfind(path, 0) == 0) : (used == path)); };
    for (const AssetSlot &slot : GetAssetSlots())
    {
        const AssetBinding *binding = m_state.manifest.Find(slot.id);
        if (binding == nullptr) continue;
        AssetBinding &edit = m_state.manifest.Edit(slot.id);
        if (matches(binding->model)) { edit.model.clear(); cleared++; }
        if (matches(binding->texture)) { edit.texture.clear(); cleared++; }
    }

    // Drop every cached copy (cheap, and covers whole folders)
    m_library.Reload();
    m_library.SetManifest(m_state.manifest);
    if (cleared > 0) m_state.dirty = true;

    m_state.selectedPath = GetParentFolder(path);
    Refresh();
    m_state.SetStatus("Deleted " + path + ((cleared > 0) ? TextFormat(" and cleared %d binding%s that used it", cleared, (cleared == 1) ? "" : "s") : ""), Theme::Good);
}

void EditorApp::HandleDroppedFiles()
{
    if (!IsFileDropped()) return;
    FilePathList dropped = LoadDroppedFiles();
    std::vector<std::string> paths;
    for (unsigned int i = 0; i < dropped.count; i++) paths.push_back(AssetFiles::Normalize(dropped.paths[i]));
    UnloadDroppedFiles(dropped);

    if (m_prompt != Prompt::None) return;
    Import(paths, m_state.GetImportFolder());
}

void EditorApp::HandleShortcuts()
{
    if (Gui::IsTyping() || (m_prompt != Prompt::None) || m_importDialog.IsOpen()) return;
    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (control && IsKeyPressed(KEY_S)) Save();
    if (control && IsKeyPressed(KEY_I)) m_importDialog.Open(m_state.GetImportFolder());
    if (IsKeyPressed(KEY_F5)) Refresh();
    if (IsKeyPressed(KEY_DELETE) && !m_state.selectedPath.empty()) m_prompt = Prompt::Delete;
}

void EditorApp::SaveOnClose()
{
    // raylib cannot cancel a window close, so unsaved bindings are kept by saving them
    if (m_state.dirty) Save();
}

//----------------------------------------------------------------------------------
// Drawing
//----------------------------------------------------------------------------------
void EditorApp::DrawToolbar(Rectangle bar)
{
    DrawRectangleRec(bar, Theme::PanelTitle);
    Gui::Separator(bar.x, bar.y + bar.height - 1.0f, bar.width);
    float x = bar.x + Theme::Padding;
    float y = bar.y + 7.0f;
    float h = bar.height - 14.0f;
    auto button = [&](const char *label, float width, bool enabled = true, bool highlighted = false) {
        bool pressed = Gui::Button(Rectangle{ x, y, width, h }, label, enabled, highlighted);
        x += width + 6.0f;
        return pressed;
    };

    if (button("Import...", 120.0f, true, true)) m_importDialog.Open(m_state.GetImportFolder());
    if (button("New Folder", 130.0f))
    {
        m_prompt = Prompt::NewFolder;
        m_promptText.clear();
        Gui::Focus(m_promptText);
    }
    if (button("Delete", 90.0f, !m_state.selectedPath.empty())) m_prompt = Prompt::Delete;
    if (button("Refresh", 100.0f)) Refresh();
    x += 12.0f;
    if (button(m_state.dirty ? "Save *" : "Save", 90.0f, true, m_state.dirty)) Save();

    std::string into = m_state.GetImportFolder();
    Gui::LabelFitted(Rectangle{ x + 10.0f, bar.y, bar.x + bar.width - x - 20.0f, bar.height },
                     "Import into: " + (into.empty() ? std::string("Assets/ (sorted by type)") : "Assets/" + into) + "     Drag & drop works too", Theme::TextDim, Theme::SmallFontSize);
}

void EditorApp::DrawStatusBar(Rectangle bar)
{
    DrawRectangleRec(bar, Theme::PanelTitle);
    Gui::Separator(bar.x, bar.y, bar.width);
    float age = static_cast<float>(GetTime() - m_state.statusTime);
    Color color = (age < 4.0f) ? m_state.statusColor : Theme::TextDim;
    Gui::LabelFitted(Rectangle{ bar.x + Theme::Padding, bar.y, bar.width - 300.0f, bar.height }, m_state.status, color, Theme::SmallFontSize);
    const char *help = "Ctrl+S save   Ctrl+I import   Del delete   F frame";
    Gui::Label(bar.x + bar.width - Gui::MeasureWidth(help, Theme::SmallFontSize) - Theme::Padding, bar.y + 8.0f, help, Theme::TextDim, Theme::SmallFontSize);
}

void EditorApp::DrawPrompt()
{
    if (m_prompt == Prompt::None) return;
    Gui::SetInputEnabled(true);

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.55f));
    Rectangle window = { (GetScreenWidth() - 520.0f)*0.5f, (GetScreenHeight() - 190.0f)*0.5f, 520.0f, 190.0f };
    float x = window.x + 16.0f;
    float width = window.width - 32.0f;
    float buttonY = window.y + window.height - 44.0f;
    Rectangle left = { x + width - 290.0f, buttonY, 140.0f, Theme::RowHeight + 4.0f };
    Rectangle right = { x + width - 140.0f, buttonY, 140.0f, Theme::RowHeight + 4.0f };
    bool cancel = IsKeyPressed(KEY_ESCAPE) && !Gui::IsTyping();

    switch (m_prompt)
    {
        case Prompt::NewFolder:
        {
            Rectangle content = Gui::Panel(window, "New folder");
            std::string parent = m_state.GetImportFolder();
            Gui::Label(x, content.y + 12.0f, "Inside Assets/" + parent, Theme::TextDim);
            bool submitted = false;
            Gui::TextBox(Rectangle{ x, content.y + 42.0f, width, Theme::RowHeight + 4.0f }, m_promptText, submitted);
            bool create = Gui::Button(right, "Create", !m_promptText.empty(), true) || (submitted && !m_promptText.empty());
            if (Gui::Button(left, "Cancel") || cancel) m_prompt = Prompt::None;
            if (create)
            {
                std::string error;
                if (Importer::CreateFolder(parent, m_promptText, error))
                {
                    m_state.selectedPath = parent + m_promptText + "/";
                    m_libraryPanel.Expand(m_state.selectedPath);
                    Refresh();
                    m_state.SetStatus("Created Assets/" + m_state.selectedPath + ", imports go there now", Theme::Good);
                }
                else m_state.SetStatus(error, Theme::Warning);
                m_prompt = Prompt::None;
            }
        } break;
        case Prompt::Delete:
        {
            Gui::Panel(window, "Delete");
            const LibraryNode *node = m_state.tree.Find(m_state.selectedPath);
            std::string what = (node != nullptr) && node->IsFolder() ? TextFormat("the folder %s and its %d files", m_state.selectedPath.c_str(), m_state.tree.CountFiles(*node)) : m_state.selectedPath;
            Gui::LabelFitted(Rectangle{ x, window.y + 50.0f, width, 24.0f }, "Delete " + what + "?", Theme::Text);
            Gui::Label(x, window.y + 82.0f, "This removes the files from disk. Bindings that use them are cleared.", Theme::TextDim, Theme::SmallFontSize);
            if (Gui::Button(left, "Cancel") || cancel) m_prompt = Prompt::None;
            if (Gui::Button(right, "Delete", true, true))
            {
                m_prompt = Prompt::None;
                DeleteSelected();
            }
        } break;
        case Prompt::None: break;
    }
}

void EditorApp::Frame()
{
    float deltaTime = GetFrameTime();
    Gui::BeginFrame();
    HandleShortcuts();
    HandleDroppedFiles();

    float width = static_cast<float>(GetScreenWidth());
    float height = static_cast<float>(GetScreenHeight());
    float libraryWidth = std::min(LibraryWidth, width*0.25f);
    float bindingsWidth = std::min(BindingsWidth, width*0.3f);
    float top = Theme::ToolbarHeight;
    float bodyHeight = height - top - StatusHeight;

    BeginDrawing();
        ClearBackground(Theme::Background);

        // Panels ignore the mouse while a dialog is up
        bool modal = (m_prompt != Prompt::None) || m_importDialog.IsOpen();
        Gui::SetInputEnabled(!modal);
        DrawToolbar(Rectangle{ 0.0f, 0.0f, width, top });
        m_libraryPanel.Draw(Rectangle{ 0.0f, top, libraryWidth, bodyHeight }, m_state);
        m_viewportPanel.Draw(Rectangle{ libraryWidth, top, width - libraryWidth - bindingsWidth, bodyHeight }, m_state, deltaTime);
        m_bindingsPanel.Draw(Rectangle{ width - bindingsWidth, top, bindingsWidth, bodyHeight }, m_state);
        DrawStatusBar(Rectangle{ 0.0f, height - StatusHeight, width, StatusHeight });

        if (m_importDialog.IsOpen())
        {
            Gui::SetInputEnabled(true);
            std::string folder = m_state.GetImportFolder();
            if (m_importDialog.Draw()) Import(m_importDialog.GetPicked(), folder);
        }
        DrawPrompt();
        Gui::EndFrame();
    EndDrawing();
}
