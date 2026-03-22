#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace loaders {

enum class ModelFormat {
    Unknown = 0,
    Obj,
    Gltf
};

struct LoadedVertex {
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

struct LoadedMesh {
    std::string name;
    std::vector<LoadedVertex> vertices;
};

struct LoadedModel {
    std::vector<LoadedMesh> meshes;

    bool Empty() const { return meshes.empty(); }
};

ModelFormat ModelFormatFromName(const std::string& name);
ModelFormat ModelFormatFromExtension(const std::filesystem::path& path);
const char* ModelFormatName(ModelFormat format);

bool LoadModel(
    const std::filesystem::path& model_path,
    ModelFormat format,
    LoadedModel& out_model,
    std::string& out_error);

bool LoadModelAuto(
    const std::filesystem::path& model_path,
    LoadedModel& out_model,
    std::string& out_error);

} // namespace loaders
