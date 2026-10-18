#include "assets/AssetManifest.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace
{
    std::string Trim(const std::string &text)
    {
        size_t start = text.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = text.find_last_not_of(" \t\r\n");
        return text.substr(start, end - start + 1);
    }

    std::string LowerExtension(const std::string &path)
    {
        size_t dot = path.find_last_of('.');
        if ((dot == std::string::npos) || (path.find_first_of("/\\", dot) != std::string::npos)) return "";
        std::string extension = path.substr(dot);
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return extension;
    }

    bool HasExtensionIn(const std::string &path, const char *list)
    {
        std::string extension = LowerExtension(path);
        if (extension.empty()) return false;
        std::stringstream stream(list);
        std::string entry;
        while (std::getline(stream, entry, ';'))
        {
            if (entry == extension) return true;
        }
        return false;
    }

    Vector3 ParseVector(const std::string &value, Vector3 fallback)
    {
        std::istringstream stream(value);
        Vector3 result = fallback;
        stream >> result.x >> result.y >> result.z;
        return stream.fail() ? fallback : result;
    }

    void SetField(AssetBinding &binding, const std::string &key, const std::string &value)
    {
        try
        {
            if (key == "model") binding.model = AssetFiles::Normalize(value);
            else if (key == "texture") binding.texture = AssetFiles::Normalize(value);
            else if (key == "autoFit") binding.autoFit = (value != "0") && (value != "false");
            else if (key == "scale") binding.scale = std::stof(value);
            else if (key == "offset") binding.offset = ParseVector(value, binding.offset);
            else if (key == "rotation") binding.rotation = ParseVector(value, binding.rotation);
            else if (key == "animation") binding.animation = std::stoi(value);
            else if (key == "animationSpeed") binding.animationSpeed = std::stof(value);
        }
        catch (const std::exception &)
        {
            TraceLog(LOG_WARNING, "ASSETS: Bad value '%s' for '%s' in the asset library", value.c_str(), key.c_str());
        }
    }
}

//----------------------------------------------------------------------------------
// Files
//----------------------------------------------------------------------------------
namespace AssetFiles
{
    bool IsModel(const std::string &path) { return HasExtensionIn(path, ModelExtensions); }
    bool IsTexture(const std::string &path) { return HasExtensionIn(path, TextureExtensions); }

    std::string GetLibraryRoot()
    {
        std::string folder = Normalize(GetApplicationDirectory());
        if (!folder.empty() && (folder.back() != '/')) folder += '/';
        return folder + "Assets/";
    }

    std::string GetManifestPath()
    {
        return GetLibraryRoot() + "library.ini";
    }

    std::string ToFullPath(const std::string &libraryPath)
    {
        return GetLibraryRoot() + libraryPath;
    }

    std::string Normalize(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }
}

//----------------------------------------------------------------------------------
// Manifest
//----------------------------------------------------------------------------------
bool AssetManifest::Load(const std::string &path)
{
    m_bindings.clear();
    std::ifstream file(path);
    if (!file) return false;

    AssetBinding *current = nullptr;
    std::string line;
    while (std::getline(file, line))
    {
        line = Trim(line);
        if (line.empty() || (line[0] == '#') || (line[0] == ';')) continue;

        if ((line.front() == '[') && (line.back() == ']'))
        {
            current = &m_bindings[Trim(line.substr(1, line.size() - 2))];
            continue;
        }

        size_t equals = line.find('=');
        if ((current == nullptr) || (equals == std::string::npos)) continue;
        SetField(*current, Trim(line.substr(0, equals)), Trim(line.substr(equals + 1)));
    }
    return true;
}

bool AssetManifest::Save(const std::string &path) const
{
    std::ofstream file(path, std::ios::trunc);
    if (!file) return false;

    file << "# Raylib3D asset library: written by the asset editor (Editor/), read by the game at startup and on F8\n";
    file << "# Paths are relative to this folder. rotation is in degrees, offset in world units (view space for weapons)\n";
    for (const auto &[id, binding] : m_bindings)
    {
        if (!binding.HasModel() && binding.texture.empty()) continue;
        file << "\n[" << id << "]\n";
        file << "model = " << binding.model << "\n";
        file << "texture = " << binding.texture << "\n";
        file << "autoFit = " << (binding.autoFit ? 1 : 0) << "\n";
        file << "scale = " << binding.scale << "\n";
        file << "offset = " << binding.offset.x << " " << binding.offset.y << " " << binding.offset.z << "\n";
        file << "rotation = " << binding.rotation.x << " " << binding.rotation.y << " " << binding.rotation.z << "\n";
        file << "animation = " << binding.animation << "\n";
        file << "animationSpeed = " << binding.animationSpeed << "\n";
    }
    return static_cast<bool>(file);
}

const AssetBinding *AssetManifest::Find(const std::string &slotId) const
{
    auto found = m_bindings.find(slotId);
    return (found != m_bindings.end()) ? &found->second : nullptr;
}

AssetBinding &AssetManifest::Edit(const std::string &slotId)
{
    return m_bindings[slotId];
}

void AssetManifest::Clear(const std::string &slotId)
{
    m_bindings.erase(slotId);
}
