#pragma once

#include "raylib.h"
#include "assets/AssetLibrary.h"
#include <string>

struct EditorState;
struct AssetSlot;

//----------------------------------------------------------------------------------
// Center panel: model visualizer. Shows the selected library file as imported, or
// the selected slot's model placed and sized the way the game draws it (with the
// enemy's collision size for reference, or a first person view for weapons).
//   Left/right drag orbit, middle drag (or shift + drag) pan, wheel zoom, F frame
//----------------------------------------------------------------------------------
class ViewportPanel
{
public:
    ViewportPanel() = default;
    ~ViewportPanel();
    ViewportPanel(const ViewportPanel &) = delete;
    ViewportPanel &operator=(const ViewportPanel &) = delete;

    void Draw(Rectangle bounds, EditorState &state, float deltaTime);

private:
    // Everything needed to draw what is being previewed
    struct Subject
    {
        const LoadedModel *model = nullptr;
        Texture2D texture = {};
        AssetBinding binding;
        float fitSize = 2.0f;
        FitMode fitMode = FitMode::Length;
        const AssetSlot *slot = nullptr;
        std::string name;
        std::string key;            // Changes when the previewed thing changes (reframes the camera)
    };

    Subject GetSubject(EditorState &state);
    void DrawToolbar(Rectangle bar, const Subject &subject);
    void DrawAnimationBar(Rectangle bar, EditorState &state, Subject &subject);
    void UpdateOrbit(Rectangle view);
    void FrameSubject(const Subject &subject);
    Matrix GetPlacement(const Subject &subject) const;
    void RenderScene(Rectangle view, const Subject &subject, EditorState &state);
    void DrawOverlay(Rectangle view, const Subject &subject);
    void DrawTexturePreview(Rectangle view, Texture2D texture, const std::string &name);
    void DrawMessage(Rectangle view, const char *title, const char *hint);

    RenderTexture2D m_target = {};
    Camera3D m_camera = {};
    Vector3 m_orbitTarget = { 0.0f, 1.0f, 0.0f };
    float m_yaw = 0.7f;
    float m_pitch = 0.3f;
    float m_distance = 5.0f;
    std::string m_framedKey;
    bool m_dragging = false;

    bool m_showGrid = true;
    bool m_wireframe = false;
    bool m_showBounds = false;
    bool m_showReference = true;
    bool m_firstPerson = false;
    bool m_aimView = false;
    bool m_playing = true;
    float m_time = 0.0f;
    int m_fileAnimation = 0;         // Clip previewed for a library file (slots use their binding's clip)
};
