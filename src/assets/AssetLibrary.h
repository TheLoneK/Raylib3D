#pragma once

#include "raylib.h"
#include "raymath.h"
#include "assets/AssetManifest.h"
#include <map>
#include <string>
#include <string_view>

//----------------------------------------------------------------------------------
// GPU side of the asset library: loads the models/textures named in library.ini and
// draws them with a shader that matches the game's look (fog, simple light, hit
// flash) and skins animated models on the GPU. Shared by the game and the editor so
// the editor preview looks like the game.
// Needs a window (OpenGL context), so create it after InitWindow.
//----------------------------------------------------------------------------------
struct LoadedModel
{
    Model model = {};
    ModelAnimation *animations = nullptr;
    int animationCount = 0;
    BoundingBox bounds = {};        // Bind pose, model space
    std::string error;              // Why loading failed (model.meshCount == 0)

    bool IsValid() const { return model.meshCount > 0; }
};

struct AssetDrawParams
{
    Matrix placement = MatrixIdentity();       // Where the slot is (world or view space)
    float fitSize = 1.0f;
    FitMode fitMode = FitMode::Height;
    Color tint = WHITE;             // Multiplies the texture
    Color flashColor = WHITE;       // Mixed over the result by flash (hit flash, attack telegraph)
    float flash = 0.0f;
    float time = 0.0f;              // Animation clock in seconds
    bool wireframe = false;         // Editor preview
};

// World placement for a slot standing at position and facing yaw (game yaw convention: 0 looks down -Z)
inline Matrix AssetPlacement(Vector3 position, float yaw)
{
    return MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(position.x, position.y, position.z));
}

class AssetLibrary
{
public:
    AssetLibrary();
    ~AssetLibrary();
    AssetLibrary(const AssetLibrary &) = delete;
    AssetLibrary &operator=(const AssetLibrary &) = delete;

    // Game: re-read library.ini and load every bound file. Returns the number of bound slots that loaded
    int Reload();
    // Editor: use these bindings (loads files that are not cached yet)
    void SetManifest(const AssetManifest &manifest);
    const AssetManifest &GetManifest() const { return m_manifest; }
    void UnloadFile(const std::string &libraryPath);    // Drop a cached file (deleted or re-imported)

    // Cached loading by library path ("Models/brute.glb"). Never null; check IsValid / id
    const LoadedModel &GetModel(const std::string &libraryPath);
    Texture2D GetTexture(const std::string &libraryPath);

    // A slot draws its custom model only when one is bound and loaded
    bool HasModel(std::string_view slotId) const;
    void SetView(Vector3 viewPosition, Color fogColor, float fogStart, float fogEnd);     // Once per frame before drawing
    void DrawSlot(std::string_view slotId, const AssetDrawParams &params) const;
    void Draw(const LoadedModel &model, Texture2D texture, const AssetBinding &binding, const AssetDrawParams &params) const;

    // Model space -> slot space transform for a binding (rotation, auto fit, scale, offset)
    static Matrix GetBindingTransform(const LoadedModel &model, const AssetBinding &binding, float fitSize, FitMode fitMode);
    static BoundingBox GetPlacedBounds(const LoadedModel &model, const AssetBinding &binding, float fitSize, FitMode fitMode);

private:
    struct ResolvedSlot
    {
        const LoadedModel *model;
        Texture2D texture;
        AssetBinding binding;
    };

    void ResolveSlots();
    void UnloadAll();
    void FreeModel(LoadedModel &model);

    AssetManifest m_manifest;
    std::map<std::string, LoadedModel> m_models;
    std::map<std::string, Texture2D> m_textures;
    std::map<std::string, ResolvedSlot, std::less<>> m_slots;     // Looked up every draw: no string copies
    LoadedModel m_missingModel;
    Shader m_shader = {};
    bool m_hasShader = false;
    unsigned int m_defaultTextureId = 0;
    int m_locFogColor = -1;
    int m_locFogRange = -1;
    int m_locViewPosition = -1;
    int m_locFlash = -1;
    int m_locSkinning = -1;
};
