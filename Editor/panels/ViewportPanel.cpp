#include "panels/ViewportPanel.h"
#include "EditorState.h"
#include "assets/AssetSlots.h"
#include "enemies/EnemyDefs.h"
#include "ui/Gui.h"
#include "ui/Theme.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float ToolbarHeight = 34.0f;
    constexpr float AnimationBarHeight = 34.0f;
    constexpr Color GameSky = { 32, 34, 44, 255 };          // The game's fog color, behind the first person preview

    float GetRadius(BoundingBox box)
    {
        return std::max(Vector3Length(box.max - box.min)*0.5f, 0.01f);
    }

    // Largest rectangle with the game's aspect ratio inside the view
    Rectangle Letterbox(Rectangle view, float aspect)
    {
        float width = view.width;
        float height = width/aspect;
        if (height > view.height)
        {
            height = view.height;
            width = height*aspect;
        }
        return Rectangle{ view.x + (view.width - width)*0.5f, view.y + (view.height - height)*0.5f, width, height };
    }
}

ViewportPanel::~ViewportPanel()
{
    if (m_target.id != 0) UnloadRenderTexture(m_target);
}

//----------------------------------------------------------------------------------
// What to show
//----------------------------------------------------------------------------------
ViewportPanel::Subject ViewportPanel::GetSubject(EditorState &state)
{
    Subject subject;
    if (state.preview == PreviewSource::Slot)
    {
        subject.slot = FindAssetSlot(state.selectedSlot);
        if (subject.slot == nullptr) return subject;
        subject.name = subject.slot->label;
        subject.fitSize = GetAssetSlotFitSize(*subject.slot);
        subject.fitMode = GetAssetSlotFitMode(*subject.slot);

        const AssetBinding *binding = state.manifest.Find(subject.slot->id);
        if ((binding == nullptr) || !binding->HasModel()) return subject;
        subject.binding = *binding;
        subject.model = &state.library.GetModel(binding->model);
        subject.texture = state.library.GetTexture(binding->texture);
        subject.key = std::string(subject.slot->id) + "|" + binding->model;
        return subject;
    }

    if ((state.preview == PreviewSource::File) && state.IsSelectedFileKind(LibraryItemKind::Model))
    {
        // As imported: no rotation, scaled to a viewable size
        subject.name = state.selectedPath;
        subject.model = &state.library.GetModel(state.selectedPath);
        subject.binding.rotation = Vector3{ 0.0f, 0.0f, 0.0f };
        subject.binding.animation = std::min(m_fileAnimation, subject.model->animationCount - 1);
        subject.key = "file|" + state.selectedPath;
    }
    return subject;
}

Matrix ViewportPanel::GetPlacement(const Subject &subject) const
{
    if (m_firstPerson && (subject.slot != nullptr) && (subject.slot->kind == SlotKind::Weapon))
    {
        Vector3 position = m_aimView ? ViewModelLayout::AimPosition : ViewModelLayout::HipPosition;
        return MatrixTranslate(position.x, position.y, position.z);
    }
    if (subject.slot != nullptr) return MatrixIdentity();

    // Library files stand on the grid
    BoundingBox bounds = AssetLibrary::GetPlacedBounds(*subject.model, subject.binding, subject.fitSize, subject.fitMode);
    return MatrixTranslate(0.0f, -bounds.min.y, 0.0f);
}

//----------------------------------------------------------------------------------
// Camera
//----------------------------------------------------------------------------------
void ViewportPanel::FrameSubject(const Subject &subject)
{
    if ((subject.model == nullptr) || !subject.model->IsValid()) return;
    BoundingBox bounds = AssetLibrary::GetPlacedBounds(*subject.model, subject.binding, subject.fitSize, subject.fitMode);
    Matrix placement = GetPlacement(subject);
    Vector3 center = Vector3Transform((bounds.min + bounds.max)*0.5f, placement);
    float radius = GetRadius(bounds);

    m_orbitTarget = center;
    m_distance = radius/sinf(45.0f*0.5f*DEG2RAD)*1.3f;
}

void ViewportPanel::UpdateOrbit(Rectangle view)
{
    if (Gui::IsHovered(view))
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) m_distance *= powf(0.88f, wheel);
    }

    // Drags only count when they started in the view
    if ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) && Gui::IsHovered(view)) m_dragging = true;
    bool held = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    if (!held) m_dragging = false;

    if (m_dragging)
    {
        Vector2 delta = GetMouseDelta();
        bool pan = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsKeyDown(KEY_LEFT_SHIFT);
        if (pan)
        {
            Vector3 forward = Vector3Normalize(m_camera.target - m_camera.position);
            Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, Vector3{ 0.0f, 1.0f, 0.0f }));
            Vector3 up = Vector3CrossProduct(right, forward);
            float speed = m_distance*0.0016f;
            m_orbitTarget = m_orbitTarget - right*(delta.x*speed) + up*(delta.y*speed);
        }
        else
        {
            m_yaw -= delta.x*0.008f;
            m_pitch = Clamp(m_pitch + delta.y*0.008f, -1.5f, 1.5f);
        }
    }

    m_distance = Clamp(m_distance, 0.05f, 500.0f);
    Vector3 offset = { cosf(m_pitch)*sinf(m_yaw), sinf(m_pitch), cosf(m_pitch)*cosf(m_yaw) };
    m_camera.position = m_orbitTarget + offset*m_distance;
    m_camera.target = m_orbitTarget;
    m_camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    m_camera.fovy = 45.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;
}

//----------------------------------------------------------------------------------
// Drawing
//----------------------------------------------------------------------------------
void ViewportPanel::RenderScene(Rectangle view, const Subject &subject, EditorState &state)
{
    int width = std::max(static_cast<int>(view.width), 1);
    int height = std::max(static_cast<int>(view.height), 1);
    if ((m_target.id == 0) || (m_target.texture.width != width) || (m_target.texture.height != height))
    {
        if (m_target.id != 0) UnloadRenderTexture(m_target);
        m_target = LoadRenderTexture(width, height);
    }

    bool weapon = (subject.slot != nullptr) && (subject.slot->kind == SlotKind::Weapon);
    bool firstPerson = m_firstPerson && weapon;
    Camera3D camera = firstPerson ? ViewModelLayout::GetCamera() : m_camera;

    BoundingBox bounds = AssetLibrary::GetPlacedBounds(*subject.model, subject.binding, subject.fitSize, subject.fitMode);
    float radius = GetRadius(bounds);
    Matrix placement = GetPlacement(subject);

    AssetDrawParams params;
    params.placement = placement;
    params.fitSize = subject.fitSize;
    params.fitMode = subject.fitMode;
    params.time = m_time;
    params.wireframe = m_wireframe;

    BeginTextureMode(m_target);
        ClearBackground(firstPerson ? GameSky : Theme::Viewport);
        state.library.SetView(camera.position, BLACK, 10000.0f, 20000.0f);
        BeginMode3D(camera);
            if (m_showGrid && !firstPerson)
            {
                float spacing = (radius > 1.5f) ? 0.5f : ((radius > 0.3f) ? 0.1f : 0.02f);
                DrawGrid(24, spacing);
                float axis = spacing*4.0f;
                DrawLine3D(Vector3{ 0.0f, 0.001f, 0.0f }, Vector3{ axis, 0.001f, 0.0f }, RED);
                DrawLine3D(Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, axis, 0.0f }, GREEN);
                DrawLine3D(Vector3{ 0.0f, 0.001f, 0.0f }, Vector3{ 0.0f, 0.001f, axis }, BLUE);
            }

            state.library.Draw(*subject.model, subject.texture, subject.binding, params);

            if (m_showBounds) DrawBoundingBox(AssetLibrary::GetPlacedBounds(*subject.model, subject.binding, subject.fitSize, subject.fitMode), Theme::Accent);

            if ((subject.slot != nullptr) && !firstPerson && m_showReference)
            {
                // Which way the game faces the model: -Z
                float length = std::max(radius*0.9f, subject.fitSize*0.6f);
                Vector3 start = { 0.0f, 0.01f, 0.0f };
                Vector3 tip = { 0.0f, 0.01f, -length };
                DrawCylinderEx(start, tip*0.85f, length*0.015f, length*0.015f, 6, Theme::Accent);
                DrawCylinderEx(tip*0.85f, tip, length*0.05f, 0.0f, 8, Theme::Accent);

                if (subject.slot->kind == SlotKind::Enemy)
                {
                    // The enemy's collision cylinder: what bullets actually hit
                    const EnemyDef &def = GetEnemyDef(static_cast<EnemyType>(subject.slot->type));
                    DrawCylinderWires(Vector3{ 0.0f, 0.0f, 0.0f }, def.radius, def.radius, subject.fitSize, 16, Fade(Theme::Good, 0.7f));
                    if (def.headHeightFraction > 0.0f)
                    {
                        float head = subject.fitSize*def.headHeightFraction;
                        DrawCylinderWires(Vector3{ 0.0f, head, 0.0f }, def.radius, def.radius, 0.0f, 16, Fade(Theme::Warning, 0.8f));
                    }
                }
            }
        EndMode3D();

        if (firstPerson)
        {
            // Crosshair like the game's, so aim offsets can be lined up
            float cx = width*0.5f;
            float cy = height*0.5f;
            DrawLineEx(Vector2{ cx - 10.0f, cy }, Vector2{ cx - 3.0f, cy }, 2.0f, WHITE);
            DrawLineEx(Vector2{ cx + 3.0f, cy }, Vector2{ cx + 10.0f, cy }, 2.0f, WHITE);
            DrawLineEx(Vector2{ cx, cy - 10.0f }, Vector2{ cx, cy - 3.0f }, 2.0f, WHITE);
            DrawLineEx(Vector2{ cx, cy + 3.0f }, Vector2{ cx, cy + 10.0f }, 2.0f, WHITE);
        }
    EndTextureMode();

    DrawTextureRec(m_target.texture, Rectangle{ 0.0f, 0.0f, static_cast<float>(width), -static_cast<float>(height) }, Vector2{ view.x, view.y }, WHITE);

    if ((subject.slot != nullptr) && !firstPerson && m_showReference)
    {
        Vector2 label = GetWorldToScreenEx(Vector3{ 0.0f, 0.01f, -std::max(radius*0.9f, subject.fitSize*0.6f)*1.08f }, camera, width, height);
        Gui::Label(view.x + label.x - 20.0f, view.y + label.y - 6.0f, "front", Theme::Accent, Theme::SmallFontSize);
    }
}

void ViewportPanel::DrawOverlay(Rectangle view, const Subject &subject)
{
    const LoadedModel &model = *subject.model;
    int triangles = 0;
    for (int i = 0; i < model.model.meshCount; i++) triangles += model.model.meshes[i].triangleCount;
    BoundingBox placed = AssetLibrary::GetPlacedBounds(model, subject.binding, subject.fitSize, subject.fitMode);
    Vector3 size = placed.max - placed.min;
    Vector3 original = model.bounds.max - model.bounds.min;

    float y = view.y + view.height - 44.0f;
    Gui::Label(view.x + 10.0f, y, TextFormat("%d triangles  |  %d meshes  |  %d materials  |  %d animations", triangles, model.model.meshCount, model.model.materialCount, model.animationCount), Theme::TextDim, Theme::SmallFontSize);
    Gui::Label(view.x + 10.0f, y + 16.0f, TextFormat("File size %.2f x %.2f x %.2f  ->  shown %.2f x %.2f x %.2f", original.x, original.y, original.z, size.x, size.y, size.z), Theme::TextDim, Theme::SmallFontSize);

    if ((subject.slot != nullptr) && (subject.slot->kind == SlotKind::Enemy) && m_showReference)
    {
        Gui::Label(view.x + 10.0f, view.y + 8.0f, "green: hit cylinder   red ring: headshot line   arrow: facing", Theme::TextDim, Theme::SmallFontSize);
    }
    if (m_firstPerson && (subject.slot != nullptr) && (subject.slot->kind == SlotKind::Weapon))
    {
        Gui::Label(view.x + 10.0f, view.y + 8.0f, m_aimView ? "First person: aiming down sights" : "First person: hip", Theme::TextDim, Theme::SmallFontSize);
    }
}

void ViewportPanel::DrawTexturePreview(Rectangle view, Texture2D texture, const std::string &name)
{
    // Checkerboard shows transparency
    const float cell = 16.0f;
    Gui::PushClip(view);
    for (float y = view.y; y < view.y + view.height; y += cell)
    {
        for (float x = view.x; x < view.x + view.width; x += cell)
        {
            bool odd = (static_cast<int>((x - view.x)/cell) + static_cast<int>((y - view.y)/cell)) % 2;
            DrawRectangleRec(Rectangle{ x, y, cell, cell }, odd ? Color{ 58, 60, 70, 255 } : Color{ 48, 50, 58, 255 });
        }
    }

    if (texture.id == 0)
    {
        Gui::PopClip();
        DrawMessage(view, "This texture could not be loaded", "raylib may not have this image format enabled");
        return;
    }

    float scale = std::min((view.width - 40.0f)/texture.width, (view.height - 70.0f)/texture.height);
    scale = std::min(scale, 8.0f);
    float width = texture.width*scale;
    float height = texture.height*scale;
    Rectangle destination = { view.x + (view.width - width)*0.5f, view.y + (view.height - height)*0.5f, width, height };
    DrawTexturePro(texture, Rectangle{ 0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height) }, destination, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
    DrawRectangleLinesEx(destination, 1.0f, Theme::Border);
    Gui::Label(view.x + 10.0f, view.y + view.height - 26.0f, TextFormat("%s  |  %d x %d  |  %d mip levels", name.c_str(), texture.width, texture.height, texture.mipmaps), Theme::TextDim, Theme::SmallFontSize);
    Gui::PopClip();
}

void ViewportPanel::DrawMessage(Rectangle view, const char *title, const char *hint)
{
    float centerX = view.x + view.width*0.5f;
    float centerY = view.y + view.height*0.5f;
    Gui::Label(centerX - Gui::MeasureWidth(title)*0.5f, centerY - 24.0f, title, Theme::Text);
    Gui::Label(centerX - Gui::MeasureWidth(hint, Theme::SmallFontSize)*0.5f, centerY + 6.0f, hint, Theme::TextDim, Theme::SmallFontSize);
}

void ViewportPanel::DrawToolbar(Rectangle bar, const Subject &subject)
{
    DrawRectangleRec(bar, Theme::PanelTitle);
    float x = bar.x + 6.0f;
    auto toggle = [&](const char *label, bool &value, float width) {
        Gui::Toggle(Rectangle{ x, bar.y + 4.0f, width, bar.height - 8.0f }, label, value);
        x += width + 4.0f;
    };

    toggle("Grid", m_showGrid, 64.0f);
    toggle("Wireframe", m_wireframe, 116.0f);
    toggle("Bounds", m_showBounds, 84.0f);
    if (subject.slot != nullptr) toggle("Guides", m_showReference, 84.0f);
    if ((subject.slot != nullptr) && (subject.slot->kind == SlotKind::Weapon))
    {
        toggle("First person", m_firstPerson, 140.0f);
        if (m_firstPerson) toggle("Aim", m_aimView, 54.0f);
    }
    if (Gui::Button(Rectangle{ x, bar.y + 4.0f, 84.0f, bar.height - 8.0f }, "Frame", subject.model != nullptr)) m_framedKey.clear();
    x += 88.0f;

    float nameX = std::max(x + 8.0f, bar.x + bar.width - 8.0f - Gui::MeasureWidth(subject.name));
    Gui::LabelFitted(Rectangle{ nameX, bar.y, bar.x + bar.width - 8.0f - nameX, bar.height }, subject.name, Theme::TextDim);
}

void ViewportPanel::DrawAnimationBar(Rectangle bar, EditorState &state, Subject &subject)
{
    const LoadedModel &model = *subject.model;
    DrawRectangleRec(bar, Theme::PanelTitle);
    float x = bar.x + 6.0f;
    float y = bar.y + 4.0f;
    float h = bar.height - 8.0f;

    int current = subject.binding.animation;
    int next = current;
    if (Gui::Button(Rectangle{ x, y, 34.0f, h }, "<")) next = (current <= -1) ? model.animationCount - 1 : current - 1;
    x += 38.0f;
    if (Gui::Button(Rectangle{ x, y, 34.0f, h }, ">")) next = (current >= model.animationCount - 1) ? -1 : current + 1;
    x += 42.0f;

    const char *name = (current >= 0) ? TextFormat("%d/%d  %s", current + 1, model.animationCount, model.animations[current].name) : "No animation (bind pose)";
    Gui::LabelFitted(Rectangle{ x, bar.y, 300.0f, bar.height }, name, (current >= 0) ? Theme::Text : Theme::TextDim);
    x += 306.0f;
    if (Gui::Button(Rectangle{ x, y, 90.0f, h }, m_playing ? "Pause" : "Play")) m_playing = !m_playing;
    x += 96.0f;
    if (current >= 0)
    {
        const ModelAnimation &animation = model.animations[current];
        Gui::Label(x, bar.y + (bar.height - Theme::SmallFontSize)*0.5f, TextFormat("%d frames  %.2fs", animation.keyframeCount, animation.keyframeCount/60.0f), Theme::TextDim, Theme::SmallFontSize);
    }

    if (next == current) return;
    if (subject.slot != nullptr)
    {
        // Slots keep their clip in the binding: this is what the game will play
        state.manifest.Edit(subject.slot->id).animation = next;
        state.MarkChanged();
        subject.binding.animation = next;
    }
    else
    {
        m_fileAnimation = next;
        subject.binding.animation = next;
    }
    m_time = 0.0f;
}

void ViewportPanel::Draw(Rectangle bounds, EditorState &state, float deltaTime)
{
    Rectangle content = Gui::Panel(bounds, "Viewport");
    Subject subject = GetSubject(state);
    if (m_playing) m_time += deltaTime;

    DrawToolbar(Rectangle{ content.x, content.y, content.width, ToolbarHeight }, subject);
    Rectangle view = { content.x, content.y + ToolbarHeight, content.width, content.height - ToolbarHeight };

    // Library textures
    if ((state.preview == PreviewSource::File) && state.IsSelectedFileKind(LibraryItemKind::Texture))
    {
        DrawTexturePreview(view, state.library.GetTexture(state.selectedPath), state.selectedPath);
        return;
    }

    DrawRectangleRec(view, Theme::Viewport);
    if ((subject.model == nullptr) && (subject.slot != nullptr))
    {
        DrawMessage(view, TextFormat("%s uses its built-in look", subject.slot->label), "Select a model in the Library, then press 'Use selected' next to Model in the Bindings panel");
        return;
    }
    if (subject.model == nullptr)
    {
        DrawMessage(view, "Nothing to preview", "Select a model or texture in the Library, or an enemy/weapon in the Bindings panel");
        return;
    }
    if (!subject.model->IsValid())
    {
        DrawMessage(view, "This model could not be loaded", subject.model->error.c_str());
        return;
    }

    bool hasAnimations = subject.model->animationCount > 0;
    if (hasAnimations)
    {
        view.height -= AnimationBarHeight;
        DrawAnimationBar(Rectangle{ view.x, view.y + view.height, view.width, AnimationBarHeight }, state, subject);
    }

    // First person previews use the game's screen shape, so the gun sits where it will in game
    bool firstPerson = m_firstPerson && (subject.slot != nullptr) && (subject.slot->kind == SlotKind::Weapon);
    Rectangle scene = firstPerson ? Letterbox(view, 16.0f/9.0f) : view;

    if (subject.key != m_framedKey)
    {
        FrameSubject(subject);
        m_framedKey = subject.key;
    }
    if (IsKeyPressed(KEY_F) && !Gui::IsTyping() && Gui::IsInputEnabled()) FrameSubject(subject);
    if (!firstPerson) UpdateOrbit(view);

    RenderScene(scene, subject, state);
    DrawOverlay(view, subject);
}
