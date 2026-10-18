#pragma once

#include "raylib.h"
#include <map>
#include <string>

//----------------------------------------------------------------------------------
// Asset library file (Assets/library.ini): which imported model/texture each asset
// slot uses and how it is placed. Plain data, no GPU work, so the editor and the
// game share it. Paths are relative to the library root with '/' separators.
//----------------------------------------------------------------------------------
// How a model is scaled to its slot's size when AssetBinding::autoFit is on
enum class FitMode
{
    Height,     // Enemies: model height = fit size, feet on the origin
    Length      // Weapons: longest side = fit size, centered on the origin
};

struct AssetBinding
{
    std::string model;                  // "" = keep the built-in look
    std::string texture;                // Replaces the model's diffuse texture ("" = the model's own)
    bool autoFit = true;                // Scale to the slot's size (enemy height / weapon length) before `scale`
    float scale = 1.0f;
    Vector3 offset = { 0.0f, 0.0f, 0.0f };          // Enemies: world units from the feet. Weapons: view space
    Vector3 rotation = { 0.0f, 180.0f, 0.0f };      // Degrees (X, then Y, then Z). 180 on Y turns +Z-facing models (glTF) forward
    int animation = -1;                 // Animation clip to loop, -1 = none
    float animationSpeed = 1.0f;

    bool HasModel() const { return !model.empty(); }
};

namespace AssetFiles
{
    // Extensions the game can load (raylib), lower case with the dot
    inline constexpr const char *ModelExtensions = ".obj;.gltf;.glb;.iqm;.vox;.m3d";
    inline constexpr const char *TextureExtensions = ".png;.jpg;.jpeg;.bmp;.tga;.gif;.qoi;.psd;.hdr;.dds";

    bool IsModel(const std::string &path);
    bool IsTexture(const std::string &path);
    std::string GetLibraryRoot();                       // <exe folder>/Assets/ (with the trailing '/')
    std::string GetManifestPath();                      // <library root>/library.ini
    std::string ToFullPath(const std::string &libraryPath);
    std::string Normalize(std::string path);            // '\' -> '/'
}

class AssetManifest
{
public:
    bool Load(const std::string &path);                 // false when the file is missing (an empty manifest is valid)
    bool Save(const std::string &path) const;

    const AssetBinding *Find(const std::string &slotId) const;
    AssetBinding &Edit(const std::string &slotId);      // Creates a default binding if needed
    void Clear(const std::string &slotId);
    const std::map<std::string, AssetBinding> &GetBindings() const { return m_bindings; }

private:
    std::map<std::string, AssetBinding> m_bindings;     // Keyed by AssetSlot::id
};
