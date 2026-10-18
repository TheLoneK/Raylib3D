#include "panels/BindingsPanel.h"
#include "EditorState.h"
#include "assets/AssetSlots.h"
#include "ui/Theme.h"
#include <string>

namespace
{
    std::string FileName(const std::string &path)
    {
        size_t slash = path.find_last_of('/');
        return (slash == std::string::npos) ? path : path.substr(slash + 1);
    }
}

float BindingsPanel::DrawSlotList(Rectangle area, EditorState &state)
{
    float y = area.y + 4.0f;
    auto header = [&](const char *text) {
        Gui::Label(area.x + Theme::Padding, y + 6.0f, text, Theme::TextDim, Theme::SmallFontSize);
        y += 22.0f;
    };

    SlotKind lastKind = SlotKind::Weapon;
    bool first = true;
    for (const AssetSlot &slot : GetAssetSlots())
    {
        if (first || (slot.kind != lastKind)) header((slot.kind == SlotKind::Enemy) ? "ENEMIES" : "WEAPONS (first person)");
        first = false;
        lastKind = slot.kind;

        Rectangle row = { area.x, y, area.width, Theme::RowHeight };
        y += Theme::RowHeight;
        if (Gui::SelectableRow(row, state.selectedSlot == slot.id))
        {
            state.selectedSlot = slot.id;
            state.preview = PreviewSource::Slot;
        }

        const AssetBinding *binding = state.manifest.Find(slot.id);
        bool custom = (binding != nullptr) && binding->HasModel();
        bool broken = custom && !state.library.GetModel(binding->model).IsValid();
        Gui::LabelFitted(Rectangle{ row.x + Theme::Padding + 4.0f, row.y, 130.0f, row.height }, slot.label, Theme::Text);

        std::string value = custom ? FileName(binding->model) : "built-in";
        Color color = broken ? Theme::Warning : (custom ? Theme::ModelTag : Theme::TextDim);
        float valueX = row.x + 150.0f;
        Gui::LabelFitted(Rectangle{ valueX, row.y, row.x + row.width - valueX - Theme::Padding, row.height }, broken ? "missing: " + value : value, color);
    }
    return y - area.y + 4.0f;
}

void BindingsPanel::DrawInspector(Rectangle area, EditorState &state, const AssetSlot &slot)
{
    // Edited in place: the widgets track the values they drag by address, so it must stay put between frames
    // (empty bindings are left out when saving)
    AssetBinding &binding = state.manifest.Edit(slot.id);
    bool changed = false;

    float x = area.x + Theme::Padding;
    float width = area.width - Theme::Padding*2.0f - 10.0f;     // Room for the scrollbar
    float y = area.y + 6.0f - m_inspectorScroll.offset;
    float row = Theme::RowHeight + 4.0f;

    Gui::BeginScroll(area, m_inspectorScroll, m_inspectorHeight);

    Gui::Label(x, y, slot.label, Theme::Accent);
    Gui::Label(x + Gui::MeasureWidth(slot.label) + 10.0f, y + 6.0f, slot.id, Theme::TextDim, Theme::SmallFontSize);
    y += row;

    // Model and texture pickers: "Use selected" takes the file selected in the library
    auto filePicker = [&](const char *label, std::string &path, LibraryItemKind kind, const char *emptyText) {
        Gui::Label(x, y + 4.0f, label, Theme::TextDim);
        y += 24.0f;
        Rectangle field = { x, y, width, Theme::RowHeight };
        DrawRectangleRec(field, Theme::Field);
        DrawRectangleLinesEx(field, 1.0f, Theme::Border);
        bool missing = !path.empty() && !FileExists(AssetFiles::ToFullPath(path).c_str());
        Color color = missing ? Theme::Warning : (path.empty() ? Theme::TextDim : ((kind == LibraryItemKind::Model) ? Theme::ModelTag : Theme::TextureTag));
        Gui::LabelFitted(Rectangle{ field.x + 6.0f, field.y, field.width - 12.0f, field.height }, path.empty() ? emptyText : (missing ? "missing: " + path : path), color);
        y += row;

        bool canUse = state.IsSelectedFileKind(kind) && (state.selectedPath != path);
        float half = (width - 6.0f)*0.5f;
        bool used = Gui::Button(Rectangle{ x, y, half, Theme::RowHeight }, "Use selected", canUse, canUse);
        if (used)
        {
            path = state.selectedPath;
            changed = true;
        }
        if (Gui::Button(Rectangle{ x + half + 6.0f, y, half, Theme::RowHeight }, "Clear", !path.empty()))
        {
            path.clear();
            changed = true;
        }
        y += row + 4.0f;
        return used;
    };

    std::string previousModel = binding.model;
    if (filePicker("Model", binding.model, LibraryItemKind::Model, "built-in look"))
    {
        // New model: start from a clean placement, playing its first clip
        AssetBinding fresh;
        fresh.model = binding.model;
        fresh.texture = binding.texture;
        fresh.animation = (state.library.GetModel(binding.model).animationCount > 0) ? 0 : -1;
        binding = fresh;
    }
    filePicker("Texture (replaces the model's own)", binding.texture, LibraryItemKind::Texture, "model's own textures");
    if (!binding.HasModel() && !binding.texture.empty())
    {
        Gui::Label(x, y, "A texture is only used together with a model", Theme::Warning, Theme::SmallFontSize);
        y += 20.0f;
    }

    Gui::Separator(x, y, width);
    y += 10.0f;

    // Placement
    Gui::Label(x, y, "Placement", Theme::TextDim);
    y += 26.0f;
    changed |= Gui::Checkbox(Rectangle{ x, y, width, Theme::RowHeight }, (slot.kind == SlotKind::Enemy) ? "Auto fit to the enemy's height" : "Auto fit to the viewmodel size", binding.autoFit);
    y += row;
    changed |= Gui::Slider(Rectangle{ x, y, width, Theme::RowHeight }, "Scale", binding.scale, 0.05f, 4.0f, "%.2f");
    y += row;

    float offsetSpeed = (slot.kind == SlotKind::Weapon) ? 0.0005f : 0.005f;
    const char *axes[3] = { "Offset X", "Offset Y", "Offset Z" };
    float *offsets[3] = { &binding.offset.x, &binding.offset.y, &binding.offset.z };
    for (int i = 0; i < 3; i++)
    {
        changed |= Gui::DragFloat(Rectangle{ x, y, width, Theme::RowHeight }, axes[i], *offsets[i], offsetSpeed);
        y += row;
    }

    const char *rotations[3] = { "Rotate X", "Rotate Y", "Rotate Z" };
    float *angles[3] = { &binding.rotation.x, &binding.rotation.y, &binding.rotation.z };
    for (int i = 0; i < 3; i++)
    {
        float buttonWidth = 44.0f;
        Rectangle slider = { x, y, width - buttonWidth*2.0f - 8.0f, Theme::RowHeight };
        changed |= Gui::Slider(slider, rotations[i], *angles[i], -180.0f, 180.0f, "%.0f deg");
        if (Gui::Button(Rectangle{ slider.x + slider.width + 4.0f, y, buttonWidth, Theme::RowHeight }, "-90"))
        {
            *angles[i] -= 90.0f;
            changed = true;
        }
        if (Gui::Button(Rectangle{ slider.x + slider.width + buttonWidth + 8.0f, y, buttonWidth, Theme::RowHeight }, "+90"))
        {
            *angles[i] += 90.0f;
            changed = true;
        }
        if (*angles[i] > 180.0f) *angles[i] -= 360.0f;
        if (*angles[i] < -180.0f) *angles[i] += 360.0f;
        y += row;
    }

    // Animation (the clip itself is picked under the viewport)
    const LoadedModel &model = state.library.GetModel(binding.model);
    if (model.animationCount > 0)
    {
        changed |= Gui::Slider(Rectangle{ x, y, width, Theme::RowHeight }, "Anim speed", binding.animationSpeed, 0.0f, 3.0f, "%.2fx");
        y += row;
        const char *clip = (binding.animation >= 0) && (binding.animation < model.animationCount) ? model.animations[binding.animation].name : "none";
        Gui::LabelFitted(Rectangle{ x, y, width, 20.0f }, TextFormat("Clip: %s (change it under the viewport)", clip), Theme::TextDim, Theme::SmallFontSize);
        y += 22.0f;
    }

    if (Gui::Button(Rectangle{ x, y + 4.0f, width, Theme::RowHeight }, "Reset placement", binding.HasModel()))
    {
        AssetBinding defaults;
        binding.autoFit = defaults.autoFit;
        binding.scale = defaults.scale;
        binding.offset = defaults.offset;
        binding.rotation = defaults.rotation;
        changed = true;
    }
    y += row + 8.0f;

    const char *hint = (slot.kind == SlotKind::Enemy)
        ? "Rotate until the model looks along the arrow.\nDouble click a number to type it. Shift = fine drag."
        : "Use 'First person' above the viewport to line it up.\nDouble click a number to type it. Shift = fine drag.";
    Gui::Label(x, y, hint, Theme::TextDim, Theme::SmallFontSize);
    y += 36.0f;

    Gui::EndScroll();
    m_inspectorHeight = y + m_inspectorScroll.offset - area.y;

    if (!changed) return;
    state.MarkChanged();
    if (binding.model != previousModel) state.preview = PreviewSource::Slot;
}

void BindingsPanel::Draw(Rectangle bounds, EditorState &state)
{
    Rectangle content = Gui::Panel(bounds, "Bindings");
    float used = DrawSlotList(content, state);
    Gui::Separator(content.x, content.y + used, content.width);

    Rectangle inspector = { content.x, content.y + used + 1.0f, content.width, content.height - used - 1.0f };
    const AssetSlot *slot = FindAssetSlot(state.selectedSlot);
    if (slot == nullptr)
    {
        Gui::Label(inspector.x + Theme::Padding, inspector.y + 12.0f, "Select an enemy or weapon above", Theme::TextDim);
        return;
    }
    DrawInspector(inspector, state, *slot);
}
