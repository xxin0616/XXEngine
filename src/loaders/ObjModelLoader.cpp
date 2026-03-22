#include "LoaderBackends.h"

#include "OBJ_Loader.h"

#include <filesystem>
#include <utility>

namespace loaders::detail {

bool LoadObjModel(
    const std::filesystem::path& model_path,
    LoadedModel& out_model,
    std::string& out_error) {
    out_model.meshes.clear();
    out_error.clear();

    if (!std::filesystem::exists(model_path)) {
        out_error = "OBJ file does not exist: " + model_path.string();
        return false;
    }

    objl::Loader loader;
    if (!loader.LoadFile(model_path.generic_string()) || loader.LoadedMeshes.empty()) {
        out_error = "Failed to parse OBJ file: " + model_path.string();
        return false;
    }

    for (const auto& mesh : loader.LoadedMeshes) {
        LoadedMesh out_mesh;
        out_mesh.name = mesh.MeshName;

        if (!mesh.Indices.empty()) {
            out_mesh.vertices.reserve(mesh.Indices.size());
            for (const unsigned int idx : mesh.Indices) {
                if (idx >= mesh.Vertices.size())
                    continue;
                const auto& in_v = mesh.Vertices[idx];
                LoadedVertex v;
                v.px = in_v.Position.X;
                v.py = in_v.Position.Y;
                v.pz = in_v.Position.Z;
                v.nx = in_v.Normal.X;
                v.ny = in_v.Normal.Y;
                v.nz = in_v.Normal.Z;
                v.u = in_v.TextureCoordinate.X;
                v.v = in_v.TextureCoordinate.Y;
                out_mesh.vertices.push_back(v);
            }
        }
        else {
            out_mesh.vertices.reserve(mesh.Vertices.size());
            for (const auto& in_v : mesh.Vertices) {
                LoadedVertex v;
                v.px = in_v.Position.X;
                v.py = in_v.Position.Y;
                v.pz = in_v.Position.Z;
                v.nx = in_v.Normal.X;
                v.ny = in_v.Normal.Y;
                v.nz = in_v.Normal.Z;
                v.u = in_v.TextureCoordinate.X;
                v.v = in_v.TextureCoordinate.Y;
                out_mesh.vertices.push_back(v);
            }
        }

        if (!out_mesh.vertices.empty())
            out_model.meshes.push_back(std::move(out_mesh));
    }

    if (out_model.meshes.empty()) {
        out_error = "OBJ has no usable mesh data: " + model_path.string();
        return false;
    }

    return true;
}

} // namespace loaders::detail
