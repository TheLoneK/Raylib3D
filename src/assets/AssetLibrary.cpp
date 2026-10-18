#include "assets/AssetLibrary.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace
{
    constexpr int MaxBones = 128;               // Must match MAX_BONES in the vertex shader
    constexpr float AnimationFps = 60.0f;       // raylib samples glTF animations at 60 frames per second
    constexpr int MaterialMapCount = MATERIAL_MAP_BRDF + 1;     // raylib's MAX_MATERIAL_MAPS (not in raylib.h)

    // GLSL 330 (desktop OpenGL 3.3, raylib's default). Same uniform/attribute names raylib binds automatically
    const char *VertexShader = R"(
#version 330
#define MAX_BONES 128
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 boneMatrices[MAX_BONES];
uniform int useSkinning;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
    vec4 position = vec4(vertexPosition, 1.0);
    vec4 normal = vec4(vertexNormal, 0.0);
    if (useSkinning == 1)
    {
        ivec4 bones = ivec4(vertexBoneIndices);
        mat4 skin = boneMatrices[bones.x]*vertexBoneWeights.x + boneMatrices[bones.y]*vertexBoneWeights.y +
                    boneMatrices[bones.z]*vertexBoneWeights.z + boneMatrices[bones.w]*vertexBoneWeights.w;
        position = skin*position;
        normal = skin*normal;
    }

    fragPosition = vec3(matModel*position);
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = vec3(matNormal*normal);
    gl_Position = mvp*position;
}
)";

    const char *FragmentShader = R"(
#version 330
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 fogColor;
uniform vec2 fogRange;      // start, end
uniform vec3 viewPosition;
uniform vec4 flash;         // rgb, amount

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord)*colDiffuse*fragColor;
    if (texel.a < 0.1) discard;     // Cutout transparency (leaves, hair cards)

    // Soft key light from above so shapes read without real lighting in the game
    float light = 1.0;
    if (dot(fragNormal, fragNormal) > 0.000001)
    {
        vec3 lightDirection = normalize(vec3(0.4, 1.0, 0.3));
        light = 0.55 + 0.45*max(dot(normalize(fragNormal), lightDirection), 0.0);
    }

    vec3 color = mix(texel.rgb*light, flash.rgb, flash.a);
    float fog = clamp((length(fragPosition - viewPosition) - fogRange.x)/max(fogRange.y - fogRange.x, 0.001), 0.0, 1.0);
    finalColor = vec4(mix(color, fogColor, fog), texel.a);
}
)";

    Vector3 ColorToVector(Color color)
    {
        return Vector3{ color.r/255.0f, color.g/255.0f, color.b/255.0f };
    }

    BoundingBox TransformBounds(BoundingBox box, Matrix transform)
    {
        BoundingBox result = { Vector3{ INFINITY, INFINITY, INFINITY }, Vector3{ -INFINITY, -INFINITY, -INFINITY } };
        for (int corner = 0; corner < 8; corner++)
        {
            Vector3 point = {
                (corner & 1) ? box.max.x : box.min.x,
                (corner & 2) ? box.max.y : box.min.y,
                (corner & 4) ? box.max.z : box.min.z,
            };
            point = Vector3Transform(point, transform);
            result.min = Vector3Min(result.min, point);
            result.max = Vector3Max(result.max, point);
        }
        return result;
    }

    Matrix GetRotation(const AssetBinding &binding)
    {
        return MatrixRotateXYZ(Vector3Scale(binding.rotation, DEG2RAD));
    }
}

//----------------------------------------------------------------------------------
// Setup
//----------------------------------------------------------------------------------
AssetLibrary::AssetLibrary()
{
    // The default material tells which texture raylib uses for "no texture" (never unloaded by us)
    Material defaultMaterial = LoadMaterialDefault();
    m_defaultTextureId = defaultMaterial.maps[MATERIAL_MAP_DIFFUSE].texture.id;
    unsigned int defaultShaderId = defaultMaterial.shader.id;
    MemFree(defaultMaterial.maps);

    m_shader = LoadShaderFromMemory(VertexShader, FragmentShader);
    m_hasShader = (m_shader.id != 0) && (m_shader.id != defaultShaderId);
    if (m_hasShader)
    {
        m_locFogColor = GetShaderLocation(m_shader, "fogColor");
        m_locFogRange = GetShaderLocation(m_shader, "fogRange");
        m_locViewPosition = GetShaderLocation(m_shader, "viewPosition");
        m_locFlash = GetShaderLocation(m_shader, "flash");
        m_locSkinning = GetShaderLocation(m_shader, "useSkinning");
        SetView(Vector3{ 0.0f, 0.0f, 0.0f }, BLACK, 1000.0f, 2000.0f);
    }
    else TraceLog(LOG_WARNING, "ASSETS: Model shader failed to compile, custom models draw unlit without fog");

    m_missingModel.error = "No model";
}

AssetLibrary::~AssetLibrary()
{
    UnloadAll();
    if (m_hasShader) UnloadShader(m_shader);
}

void AssetLibrary::FreeModel(LoadedModel &loaded)
{
    if (loaded.animations != nullptr) UnloadModelAnimations(loaded.animations, loaded.animationCount);
    if (loaded.IsValid())
    {
        // UnloadModel leaves textures alone (models may share them); these came from the model file
        std::set<unsigned int> textures;
        for (int i = 0; i < loaded.model.materialCount; i++)
        {
            for (int map = 0; map < MaterialMapCount; map++)
            {
                unsigned int id = loaded.model.materials[i].maps[map].texture.id;
                if ((id != 0) && (id != m_defaultTextureId)) textures.insert(id);
            }
        }
        for (unsigned int id : textures) UnloadTexture(Texture2D{ id, 0, 0, 0, 0 });
        UnloadModel(loaded.model);
    }
    loaded = LoadedModel{};
}

void AssetLibrary::UnloadAll()
{
    m_slots.clear();
    for (auto &[path, loaded] : m_models) FreeModel(loaded);
    for (auto &[path, texture] : m_textures) if (texture.id != 0) UnloadTexture(texture);
    m_models.clear();
    m_textures.clear();
}

void AssetLibrary::UnloadFile(const std::string &libraryPath)
{
    m_slots.clear();        // Slots point into the caches
    auto model = m_models.find(libraryPath);
    if (model != m_models.end())
    {
        FreeModel(model->second);
        m_models.erase(model);
    }
    auto texture = m_textures.find(libraryPath);
    if (texture != m_textures.end())
    {
        if (texture->second.id != 0) UnloadTexture(texture->second);
        m_textures.erase(texture);
    }
    ResolveSlots();
}

int AssetLibrary::Reload()
{
    UnloadAll();
    m_manifest.Load(AssetFiles::GetManifestPath());
    ResolveSlots();
    return static_cast<int>(m_slots.size());
}

void AssetLibrary::SetManifest(const AssetManifest &manifest)
{
    m_manifest = manifest;
    ResolveSlots();
}

void AssetLibrary::ResolveSlots()
{
    m_slots.clear();
    for (const auto &[id, binding] : m_manifest.GetBindings())
    {
        if (!binding.HasModel()) continue;
        const LoadedModel &model = GetModel(binding.model);
        if (!model.IsValid()) continue;
        Texture2D texture = binding.texture.empty() ? Texture2D{} : GetTexture(binding.texture);
        m_slots[id] = ResolvedSlot{ &model, texture, binding };
    }
}

//----------------------------------------------------------------------------------
// Loading
//----------------------------------------------------------------------------------
const LoadedModel &AssetLibrary::GetModel(const std::string &libraryPath)
{
    if (libraryPath.empty()) return m_missingModel;
    auto found = m_models.find(libraryPath);
    if (found != m_models.end()) return found->second;

    LoadedModel &loaded = m_models[libraryPath];
    std::string fullPath = AssetFiles::ToFullPath(libraryPath);
    if (!FileExists(fullPath.c_str()))
    {
        loaded.error = "File not found";
        return loaded;
    }
    if (!AssetFiles::IsModel(libraryPath))
    {
        loaded.error = "Not a supported model type";
        return loaded;
    }

    loaded.model = LoadModel(fullPath.c_str());
    if (!loaded.IsValid())
    {
        loaded.error = "raylib could not load this model";
        return loaded;
    }

    loaded.bounds = GetModelBoundingBox(loaded.model);
    if (m_hasShader)
    {
        for (int i = 0; i < loaded.model.materialCount; i++) loaded.model.materials[i].shader = m_shader;
    }

    // Animations only make sense with a skeleton that matches
    if (loaded.model.skeleton.boneCount > 0)
    {
        loaded.animations = LoadModelAnimations(fullPath.c_str(), &loaded.animationCount);
        if ((loaded.animations != nullptr) && ((loaded.animationCount == 0) || !IsModelAnimationValid(loaded.model, loaded.animations[0])))
        {
            UnloadModelAnimations(loaded.animations, loaded.animationCount);
            loaded.animations = nullptr;
            loaded.animationCount = 0;
        }
    }
    return loaded;
}

Texture2D AssetLibrary::GetTexture(const std::string &libraryPath)
{
    if (libraryPath.empty()) return Texture2D{};
    auto found = m_textures.find(libraryPath);
    if (found != m_textures.end()) return found->second;

    Texture2D texture = {};
    std::string fullPath = AssetFiles::ToFullPath(libraryPath);
    if (FileExists(fullPath.c_str()) && AssetFiles::IsTexture(libraryPath))
    {
        texture = LoadTexture(fullPath.c_str());
        if (texture.id != 0)
        {
            GenTextureMipmaps(&texture);
            SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        }
    }
    m_textures[libraryPath] = texture;
    return texture;
}

//----------------------------------------------------------------------------------
// Drawing
//----------------------------------------------------------------------------------
bool AssetLibrary::HasModel(std::string_view slotId) const
{
    return m_slots.find(slotId) != m_slots.end();
}

void AssetLibrary::SetView(Vector3 viewPosition, Color fogColor, float fogStart, float fogEnd)
{
    if (!m_hasShader) return;
    Vector3 color = ColorToVector(fogColor);
    Vector2 range = { fogStart, fogEnd };
    SetShaderValue(m_shader, m_locFogColor, &color, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_shader, m_locFogRange, &range, SHADER_UNIFORM_VEC2);
    SetShaderValue(m_shader, m_locViewPosition, &viewPosition, SHADER_UNIFORM_VEC3);
}

Matrix AssetLibrary::GetBindingTransform(const LoadedModel &model, const AssetBinding &binding, float fitSize, FitMode fitMode)
{
    Matrix rotation = GetRotation(binding);
    float scale = binding.scale;
    Vector3 anchor = { 0.0f, 0.0f, 0.0f };

    if (binding.autoFit)
    {
        // Measure the rotated model, then center it (feet on the floor for enemies)
        BoundingBox rotated = TransformBounds(model.bounds, rotation);
        Vector3 size = rotated.max - rotated.min;
        float measured = (fitMode == FitMode::Height) ? size.y : std::max({ size.x, size.y, size.z });
        if (measured > 0.0001f) scale *= fitSize/measured;

        anchor = (rotated.min + rotated.max)*0.5f;
        if (fitMode == FitMode::Height) anchor.y = rotated.min.y;
    }

    Matrix transform = MatrixMultiply(rotation, MatrixTranslate(-anchor.x, -anchor.y, -anchor.z));
    transform = MatrixMultiply(transform, MatrixScale(scale, scale, scale));
    return MatrixMultiply(transform, MatrixTranslate(binding.offset.x, binding.offset.y, binding.offset.z));
}

BoundingBox AssetLibrary::GetPlacedBounds(const LoadedModel &model, const AssetBinding &binding, float fitSize, FitMode fitMode)
{
    return TransformBounds(model.bounds, GetBindingTransform(model, binding, fitSize, fitMode));
}

void AssetLibrary::DrawSlot(std::string_view slotId, const AssetDrawParams &params) const
{
    auto found = m_slots.find(slotId);
    if (found == m_slots.end()) return;
    Draw(*found->second.model, found->second.texture, found->second.binding, params);
}

void AssetLibrary::Draw(const LoadedModel &loaded, Texture2D texture, const AssetBinding &binding, const AssetDrawParams &params) const
{
    if (!loaded.IsValid()) return;
    Model model = loaded.model;     // Shallow copy: same meshes/materials, own transform

    // Animated models share one skeleton, so pose it right before each draw
    bool animated = (binding.animation >= 0) && (binding.animation < loaded.animationCount) && (model.skeleton.boneCount <= MaxBones);
    if (animated)
    {
        const ModelAnimation &animation = loaded.animations[binding.animation];
        float frames = static_cast<float>(std::max(animation.keyframeCount - 1, 1));
        float frame = fmodf(std::max(params.time*binding.animationSpeed, 0.0f)*AnimationFps, frames);
        UpdateModelAnimation(model, animation, frame);
    }

    if (m_hasShader)
    {
        int skinning = animated ? 1 : 0;
        Vector4 flash = { params.flashColor.r/255.0f, params.flashColor.g/255.0f, params.flashColor.b/255.0f, Clamp(params.flash, 0.0f, 1.0f) };
        SetShaderValue(m_shader, m_locSkinning, &skinning, SHADER_UNIFORM_INT);
        SetShaderValue(m_shader, m_locFlash, &flash, SHADER_UNIFORM_VEC4);
    }

    // Texture override: swap the diffuse map for this draw only
    bool swapTexture = (texture.id != 0);
    Texture2D *originals = swapTexture ? static_cast<Texture2D *>(MemAlloc(sizeof(Texture2D)*model.materialCount)) : nullptr;
    if (swapTexture)
    {
        for (int i = 0; i < model.materialCount; i++)
        {
            originals[i] = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
            model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
        }
    }

    model.transform = MatrixMultiply(MatrixMultiply(model.transform, GetBindingTransform(loaded, binding, params.fitSize, params.fitMode)), params.placement);
    if (params.wireframe) DrawModelWires(model, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, params.tint);
    else DrawModel(model, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, params.tint);

    if (swapTexture)
    {
        for (int i = 0; i < model.materialCount; i++) model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = originals[i];
        MemFree(originals);
    }
}
