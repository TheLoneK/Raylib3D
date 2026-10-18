#include "panels/ImportDialog.h"
#include "assets/AssetManifest.h"
#include "ui/Theme.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    std::string s_lastDirectory;        // Reopens where the previous import came from

    std::string Lower(std::string text)
    {
        for (char &c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }

    std::string HomeDirectory()
    {
        const char *home = std::getenv("USERPROFILE");
        if (home == nullptr) home = std::getenv("HOME");
        if (home == nullptr) return AssetFiles::Normalize(GetWorkingDirectory());

        // Downloads is where new models usually are
        std::error_code error;
        fs::path downloads = fs::path(home)/"Downloads";
        return AssetFiles::Normalize(fs::is_directory(downloads, error) ? downloads.string() : std::string(home));
    }

    const char *KindTag(LibraryItemKind kind)
    {
        switch (kind)
        {
            case LibraryItemKind::Model: return "M";
            case LibraryItemKind::Texture: return "T";
            default: return "";
        }
    }
}

void ImportDialog::Open(const std::string &destinationFolder)
{
    m_open = true;
    m_destination = destinationFolder;
    m_selected.clear();
    m_picked.clear();
    m_error.clear();

    m_roots.clear();
#if defined(_WIN32)
    for (char letter = 'A'; letter <= 'Z'; letter++)
    {
        std::string drive = std::string(1, letter) + ":/";
        std::error_code error;
        if (fs::exists(drive, error)) m_roots.push_back(drive);
    }
#else
    m_roots.push_back("/");
#endif

    Navigate(s_lastDirectory.empty() ? HomeDirectory() : s_lastDirectory);
}

void ImportDialog::Navigate(const std::string &directory)
{
    std::error_code error;
    fs::path path = fs::path(directory).lexically_normal();
    if (!fs::is_directory(path, error))
    {
        m_error = "Not a folder: " + directory;
        m_pathText = m_directory;
        return;
    }

    std::vector<Entry> entries;
    for (const fs::directory_entry &item : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, error))
    {
        std::string name = item.path().filename().string();
        if (name.empty() || (name[0] == '.')) continue;
        std::error_code itemError;
        bool folder = item.is_directory(itemError);
        entries.push_back(Entry{ name, AssetFiles::Normalize(item.path().string()), folder, folder ? LibraryItemKind::Folder : GetLibraryItemKind(name) });
    }
    if (error && entries.empty())
    {
        m_error = "Cannot open " + directory + ": " + error.message();
        m_pathText = m_directory;
        return;
    }

    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        if (a.folder != b.folder) return a.folder;
        return Lower(a.name) < Lower(b.name);
    });

    m_entries = std::move(entries);
    m_directory = AssetFiles::Normalize(path.string());
    if (!m_directory.empty() && (m_directory.back() != '/')) m_directory += '/';
    m_pathText = m_directory;
    m_selected.clear();
    m_scroll.offset = 0.0f;
    m_error.clear();
    s_lastDirectory = m_directory;
}

bool ImportDialog::Passes(const Entry &entry) const
{
    if (entry.folder) return true;
    switch (m_filter)
    {
        case Filter::Supported: return entry.kind != LibraryItemKind::Other;
        case Filter::Models: return entry.kind == LibraryItemKind::Model;
        case Filter::Textures: return entry.kind == LibraryItemKind::Texture;
        case Filter::Everything: return true;
    }
    return true;
}

bool ImportDialog::Draw()
{
    if (!m_open) return false;

    float screenWidth = static_cast<float>(GetScreenWidth());
    float screenHeight = static_cast<float>(GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.55f));

    float width = std::min(900.0f, screenWidth - 40.0f);
    float height = std::min(640.0f, screenHeight - 40.0f);
    Rectangle window = { (screenWidth - width)*0.5f, (screenHeight - height)*0.5f, width, height };
    Rectangle content = Gui::Panel(window, "Import files into the library");
    float x = content.x + Theme::Padding;
    float inner = content.width - Theme::Padding*2.0f;
    float y = content.y + Theme::Padding;
    float row = Theme::RowHeight;

    // Path bar: Up, editable path, roots
    if (Gui::Button(Rectangle{ x, y, 60.0f, row }, "Up")) Navigate(AssetFiles::Normalize(fs::path(m_directory).lexically_normal().parent_path().parent_path().string()));
    if (Gui::Button(Rectangle{ x + 64.0f, y, 80.0f, row }, "Home")) Navigate(HomeDirectory());
    bool submitted = false;
    Gui::TextBox(Rectangle{ x + 148.0f, y, inner - 148.0f, row }, m_pathText, submitted);
    if (submitted) Navigate(m_pathText);
    y += row + 6.0f;

    float rootX = x;
    for (const std::string &root : m_roots)
    {
        if (Gui::Button(Rectangle{ rootX, y, 52.0f, 22.0f }, root.c_str())) Navigate(root);
        rootX += 56.0f;
    }

    // Filter buttons on the right of the same line
    const char *filters[] = { "Supported", "Models", "Textures", "All files" };
    float filterX = x + inner;
    for (int i = 3; i >= 0; i--)
    {
        float w = static_cast<float>(Gui::MeasureWidth(filters[i], Theme::SmallFontSize)) + 20.0f;
        filterX -= w;
        bool active = (static_cast<int>(m_filter) == i);
        if (Gui::Button(Rectangle{ filterX, y, w, 22.0f }, "", true, active)) m_filter = static_cast<Filter>(i);
        Gui::Label(filterX + 10.0f, y + 6.0f, filters[i], active ? Theme::Text : Theme::TextDim, Theme::SmallFontSize);
        filterX -= 4.0f;
    }
    y += 28.0f;

    // File list
    float footer = 112.0f;
    Rectangle list = { x, y, inner, content.y + content.height - footer - y };
    DrawRectangleRec(list, Theme::Field);
    int shown = 0;
    for (const Entry &entry : m_entries) if (Passes(entry)) shown++;

    std::string openFolder;
    Gui::BeginScroll(list, m_scroll, static_cast<float>(shown)*row);
        int index = 0;
        for (const Entry &entry : m_entries)
        {
            if (!Passes(entry)) continue;
            Rectangle line = { list.x, list.y + static_cast<float>(index)*row - m_scroll.offset, list.width - 10.0f, row };
            index++;
            if ((line.y + row < list.y) || (line.y > list.y + list.height)) continue;

            bool selected = m_selected.contains(entry.path);
            bool importable = entry.folder || (entry.kind != LibraryItemKind::Other);
            if (Gui::SelectableRow(line, selected))
            {
                if (entry.folder) openFolder = entry.path;      // Folders open with one click
                else if (importable)
                {
                    if (selected) m_selected.erase(entry.path);
                    else m_selected.insert(entry.path);
                }
            }

            float textY = line.y + (row - Theme::FontSize)*0.5f;
            if (entry.folder)
            {
                DrawRectangleRec(Rectangle{ line.x + 10.0f, line.y + 7.0f, 7.0f, 3.0f }, Theme::Accent);
                DrawRectangleRec(Rectangle{ line.x + 10.0f, line.y + 9.0f, 16.0f, 11.0f }, Theme::Accent);
            }
            else if (importable)
            {
                Rectangle box = { line.x + 10.0f, line.y + 5.0f, 16.0f, 16.0f };
                DrawRectangleLinesEx(box, 1.0f, Theme::Border);
                if (selected) DrawRectangleRec(Rectangle{ box.x + 3.0f, box.y + 3.0f, 10.0f, 10.0f }, Theme::Accent);
                Gui::Label(line.x + 36.0f, textY, KindTag(entry.kind), (entry.kind == LibraryItemKind::Model) ? Theme::ModelTag : Theme::TextureTag);
            }
            Gui::LabelFitted(Rectangle{ line.x + 60.0f, line.y, line.width - 70.0f, row }, entry.name + (entry.folder ? "/" : ""), importable ? Theme::Text : Theme::TextDim);
        }
    Gui::EndScroll();
    if (shown == 0) Gui::Label(list.x + 12.0f, list.y + 12.0f, "No supported files here", Theme::TextDim);
    if (!openFolder.empty()) Navigate(openFolder);

    // Footer: where files go, what is supported, actions
    y = content.y + content.height - footer + 8.0f;
    std::string destination = m_destination.empty() ? "Assets/Models/ and Assets/Textures/ (by type)" : "Assets/" + m_destination;
    Gui::LabelFitted(Rectangle{ x, y, inner, 22.0f }, "Into: " + destination + "   (pick another folder in the Library panel)", Theme::TextDim);
    y += 22.0f;
    Gui::LabelFitted(Rectangle{ x, y, inner, 16.0f }, std::string("Models: ") + AssetFiles::ModelExtensions + "    Textures: " + AssetFiles::TextureExtensions, Theme::TextDim, Theme::SmallFontSize);
    y += 18.0f;
    if (!m_error.empty()) Gui::LabelFitted(Rectangle{ x, y, inner, 16.0f }, m_error, Theme::Warning, Theme::SmallFontSize);
    y += 20.0f;

    float buttonWidth = 170.0f;
    bool confirmed = false;
    std::string folderName = fs::path(m_directory).lexically_normal().parent_path().filename().string();
    if (Gui::Button(Rectangle{ x, y, 300.0f, row }, ("Import folder " + folderName + "/").c_str(), !folderName.empty()))
    {
        m_picked = { m_directory };
        confirmed = true;
    }
    if (Gui::Button(Rectangle{ x + inner - buttonWidth*2.0f - 6.0f, y, buttonWidth, row }, "Cancel") || (IsKeyPressed(KEY_ESCAPE) && !Gui::IsTyping())) m_open = false;
    int count = static_cast<int>(m_selected.size());
    if (Gui::Button(Rectangle{ x + inner - buttonWidth, y, buttonWidth, row }, TextFormat("Import %d file%s", count, (count == 1) ? "" : "s"), count > 0, count > 0))
    {
        m_picked.assign(m_selected.begin(), m_selected.end());
        confirmed = true;
    }

    if (confirmed) m_open = false;
    return confirmed;
}
