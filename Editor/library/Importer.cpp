#include "library/Importer.h"
#include "assets/AssetManifest.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace
{
    std::string LibraryPathOf(const fs::path &fullPath)
    {
        std::error_code error;
        fs::path relative = fs::relative(fullPath, fs::path(AssetFiles::GetLibraryRoot()), error);
        return AssetFiles::Normalize(relative.generic_string());
    }

    // "%20" -> " " (glTF uris are URL encoded)
    std::string DecodeUri(const std::string &uri)
    {
        std::string result;
        for (size_t i = 0; i < uri.size(); i++)
        {
            if ((uri[i] == '%') && (i + 2 < uri.size()) && std::isxdigit(static_cast<unsigned char>(uri[i + 1])) && std::isxdigit(static_cast<unsigned char>(uri[i + 2])))
            {
                result += static_cast<char>(std::stoi(uri.substr(i + 1, 2), nullptr, 16));
                i += 2;
            }
            else result += uri[i];
        }
        return result;
    }

    std::string ReadFile(const fs::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string Lower(std::string text)
    {
        for (char &c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }

    // Files a model refers to, relative to the model's folder
    std::vector<std::string> FindDependencies(const fs::path &source)
    {
        std::vector<std::string> dependencies;
        std::string extension = Lower(source.extension().string());

        if (extension == ".gltf")
        {
            static const std::regex uriPattern("\"uri\"\\s*:\\s*\"([^\"]+)\"");
            std::string text = ReadFile(source);
            for (std::sregex_iterator it(text.begin(), text.end(), uriPattern), end; it != end; ++it)
            {
                std::string uri = (*it)[1].str();
                if (uri.rfind("data:", 0) != 0) dependencies.push_back(DecodeUri(uri));     // Embedded data needs no file
            }
        }
        else if (extension == ".obj")
        {
            std::ifstream file(source);
            std::string line;
            while (std::getline(file, line))
            {
                if (line.rfind("mtllib", 0) != 0) continue;
                std::string library = line.substr(6);
                library.erase(0, library.find_first_not_of(" \t"));
                library.erase(library.find_last_not_of(" \t\r") + 1);
                if (library.empty()) continue;
                dependencies.push_back(library);

                // Textures named by the material file (the path is the last word: options come first)
                std::ifstream material(source.parent_path()/library);
                std::string materialLine;
                while (std::getline(material, materialLine))
                {
                    std::istringstream words(materialLine);
                    std::string keyword;
                    words >> keyword;
                    keyword = Lower(keyword);
                    if ((keyword.rfind("map_", 0) != 0) && (keyword != "bump") && (keyword != "disp") && (keyword != "norm") && (keyword != "refl")) continue;
                    std::string word, last;
                    while (words >> word) last = word;
                    if (!last.empty()) dependencies.push_back(last);
                }
            }
        }
        return dependencies;
    }

    bool CopyOne(const fs::path &from, const fs::path &to, ImportResult &result)
    {
        std::error_code error;
        fs::create_directories(to.parent_path(), error);
        fs::copy_file(from, to, fs::copy_options::overwrite_existing, error);
        if (error)
        {
            result.messages.push_back("Could not copy " + from.filename().string() + ": " + error.message());
            result.failed++;
            return false;
        }
        result.copiedFiles++;
        return true;
    }

    void ImportFile(const fs::path &source, const fs::path &destinationFolder, ImportResult &result)
    {
        fs::path target = destinationFolder/source.filename();
        if (!CopyOne(source, target, result)) return;
        result.imported.push_back(LibraryPathOf(target));

        for (const std::string &dependency : FindDependencies(source))
        {
            fs::path relative = fs::path(dependency).lexically_normal();
            fs::path from = source.parent_path()/relative;
            if (!fs::exists(from))
            {
                result.messages.push_back(source.filename().string() + " needs " + dependency + ", which was not found");
                continue;
            }

            // Keep the same relative layout so the model's references still work
            bool escapes = relative.is_absolute() || (!relative.empty() && (*relative.begin() == ".."));
            if (escapes)
            {
                result.messages.push_back(source.filename().string() + " points outside its folder (" + dependency + "): copied next to it, fix the path if it does not load");
                relative = relative.filename();
            }
            CopyOne(from, destinationFolder/relative, result);
        }
    }

    void ImportFolder(const fs::path &source, const fs::path &destinationFolder, ImportResult &result)
    {
        fs::path target = destinationFolder/source.filename();
        std::error_code error;
        for (const fs::directory_entry &entry : fs::recursive_directory_iterator(source, error))
        {
            if (!entry.is_regular_file()) continue;
            fs::path to = target/fs::relative(entry.path(), source, error);
            if (!CopyOne(entry.path(), to, result)) continue;
            std::string libraryPath = LibraryPathOf(to);
            if (AssetFiles::IsModel(libraryPath) || AssetFiles::IsTexture(libraryPath)) result.imported.push_back(libraryPath);
        }
    }

    bool FolderHasModels(const fs::path &folder)
    {
        std::error_code error;
        for (const fs::directory_entry &entry : fs::recursive_directory_iterator(folder, error))
        {
            if (AssetFiles::IsModel(entry.path().string())) return true;
        }
        return false;
    }

    bool IsValidName(const std::string &name)
    {
        if (name.empty() || (name == ".") || (name == "..")) return false;
        return name.find_first_of("/\\:*?\"<>|") == std::string::npos;
    }
}

namespace Importer
{
    bool IsImportable(const std::string &path)
    {
        return AssetFiles::IsModel(path) || AssetFiles::IsTexture(path) || DirectoryExists(path.c_str());
    }

    std::string GetDefaultFolder(const std::string &sourcePath)
    {
        if (DirectoryExists(sourcePath.c_str())) return FolderHasModels(fs::path(sourcePath)) ? "Models/" : "Textures/";
        return AssetFiles::IsModel(sourcePath) ? "Models/" : "Textures/";
    }

    ImportResult Import(const std::vector<std::string> &sourcePaths, const std::string &destinationFolder)
    {
        ImportResult result;
        fs::path root = fs::path(AssetFiles::GetLibraryRoot());
        std::set<std::string> seen;

        for (std::string sourcePath : sourcePaths)
        {
            while ((sourcePath.size() > 1) && ((sourcePath.back() == '/') || (sourcePath.back() == '\\'))) sourcePath.pop_back();
            fs::path source = fs::path(sourcePath).lexically_normal();
            if (!seen.insert(source.string()).second) continue;

            if (!Importer::IsImportable(sourcePath))
            {
                result.messages.push_back("Skipped " + source.filename().string() + " (not a supported model or texture)");
                result.failed++;
                continue;
            }

            std::string folder = destinationFolder.empty() ? GetDefaultFolder(sourcePath) : destinationFolder;
            fs::path destination = root/fs::path(folder);

            // Importing a file that already lives in that folder would copy it onto itself
            std::error_code error;
            if (fs::equivalent(source.parent_path(), destination, error))
            {
                result.messages.push_back(source.filename().string() + " is already in the library");
                continue;
            }

            if (fs::is_directory(source)) ImportFolder(source, destination, result);
            else ImportFile(source, destination, result);
        }
        return result;
    }

    bool CreateFolder(const std::string &parentFolder, const std::string &name, std::string &error)
    {
        if (!IsValidName(name))
        {
            error = "Folder names cannot be empty or contain / \\ : * ? \" < > |";
            return false;
        }
        fs::path path = fs::path(AssetFiles::GetLibraryRoot())/fs::path(parentFolder)/name;
        std::error_code code;
        if (fs::exists(path, code))
        {
            error = "A file or folder named " + name + " already exists";
            return false;
        }
        fs::create_directories(path, code);
        if (code) error = code.message();
        return !code;
    }

    bool Delete(const std::string &libraryPath, std::string &error)
    {
        if (libraryPath.empty())
        {
            error = "The library root cannot be deleted";
            return false;
        }
        std::error_code code;
        fs::remove_all(fs::path(AssetFiles::GetLibraryRoot())/fs::path(libraryPath), code);
        if (code) error = code.message();
        return !code;
    }
}
