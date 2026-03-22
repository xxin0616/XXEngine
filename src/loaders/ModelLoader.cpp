#include "ModelLoader.h"

#include "LoaderBackends.h"

#include <algorithm>
#include <cctype>

namespace loaders {
namespace {
std::string ToLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}
}

ModelFormat ModelFormatFromName(const std::string& name) {
    const std::string lower = ToLowerCopy(name);
    if (lower == "obj")
        return ModelFormat::Obj;
    if (lower == "gltf" || lower == "glb")
        return ModelFormat::Gltf;
    return ModelFormat::Unknown;
}

ModelFormat ModelFormatFromExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.')
        ext = ext.substr(1);
    return ModelFormatFromName(ext);
}

const char* ModelFormatName(ModelFormat format) {
    switch (format) {
    case ModelFormat::Obj: return "obj";
    case ModelFormat::Gltf: return "gltf";
    case ModelFormat::Unknown:
    default: return "unknown";
    }
}

bool LoadModel(
    const std::filesystem::path& model_path,
    ModelFormat format,
    LoadedModel& out_model,
    std::string& out_error) {
    out_model.meshes.clear();
    out_error.clear();

    switch (format) {
    case ModelFormat::Obj:
        return detail::LoadObjModel(model_path, out_model, out_error);
    case ModelFormat::Gltf:
        return detail::LoadGltfModel(model_path, out_model, out_error);
    case ModelFormat::Unknown:
    default:
        out_error = "Unknown model format.";
        return false;
    }
}

bool LoadModelAuto(
    const std::filesystem::path& model_path,
    LoadedModel& out_model,
    std::string& out_error) {
    const ModelFormat inferred = ModelFormatFromExtension(model_path);
    return LoadModel(model_path, inferred, out_model, out_error);
}

} // namespace loaders
