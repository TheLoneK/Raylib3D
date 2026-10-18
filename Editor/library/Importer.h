#pragma once

#include <string>
#include <vector>

//----------------------------------------------------------------------------------
// Copies files from anywhere on disk into the asset library. Models bring the files
// they reference along (.obj -> .mtl -> textures, .gltf -> .bin/textures) so they
// still load from their new home. Folders are copied whole.
//----------------------------------------------------------------------------------
struct ImportResult
{
    std::vector<std::string> imported;      // Library paths of the models/textures added
    std::vector<std::string> messages;      // Warnings and errors, for the status bar
    int copiedFiles = 0;                    // Including companion files
    int failed = 0;
};

namespace Importer
{
    // destinationFolder is a library folder ("Models/"); "" sorts files into Models/ and Textures/
    ImportResult Import(const std::vector<std::string> &sourcePaths, const std::string &destinationFolder);
    bool IsImportable(const std::string &path);                     // Model, texture or folder
    std::string GetDefaultFolder(const std::string &sourcePath);    // Where "" sends this file

    bool CreateFolder(const std::string &parentFolder, const std::string &name, std::string &error);
    bool Delete(const std::string &libraryPath, std::string &error);
}
