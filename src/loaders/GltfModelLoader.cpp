#include "LoaderBackends.h"

#include "tiny_gltf.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace loaders::detail {
namespace {

bool ReadIndices(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<int>& out_indices,
    std::string& out_error) {
    out_indices.clear();

    if (accessor.count <= 0) {
        out_error = "Index accessor has no data.";
        return false;
    }
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        out_error = "Index accessor has invalid bufferView.";
        return false;
    }

    const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) {
        out_error = "Index bufferView has invalid buffer.";
        return false;
    }
    const tinygltf::Buffer& buffer = model.buffers[static_cast<size_t>(view.buffer)];

    const size_t elem_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    if (elem_size == 0) {
        out_error = "Unsupported index component type.";
        return false;
    }
    const size_t stride = accessor.ByteStride(view) > 0 ? static_cast<size_t>(accessor.ByteStride(view)) : elem_size;
    const size_t base_offset = static_cast<size_t>(view.byteOffset) + static_cast<size_t>(accessor.byteOffset);

    if (base_offset >= buffer.data.size()) {
        out_error = "Index data offset out of range.";
        return false;
    }

    out_indices.reserve(static_cast<size_t>(accessor.count));
    for (size_t i = 0; i < static_cast<size_t>(accessor.count); ++i) {
        const size_t offset = base_offset + i * stride;
        if (offset + elem_size > buffer.data.size()) {
            out_error = "Index data out of range.";
            return false;
        }

        int idx = 0;
        switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            idx = static_cast<int>(buffer.data[offset]);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            uint16_t value = 0;
            std::memcpy(&value, &buffer.data[offset], sizeof(uint16_t));
            idx = static_cast<int>(value);
            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            uint32_t value = 0;
            std::memcpy(&value, &buffer.data[offset], sizeof(uint32_t));
            idx = static_cast<int>(value);
            break;
        }
        default:
            out_error = "Unsupported glTF index component type.";
            return false;
        }
        out_indices.push_back(idx);
    }

    return true;
}

bool ReadFloatVec(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    int expected_type,
    int expected_components,
    std::vector<float>& out_values,
    std::string& out_error) {
    out_values.clear();

    if (accessor.type != expected_type) {
        out_error = "Attribute type mismatch.";
        return false;
    }
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        out_error = "Only float attributes are currently supported.";
        return false;
    }
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        out_error = "Attribute accessor has invalid bufferView.";
        return false;
    }

    const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) {
        out_error = "Attribute bufferView has invalid buffer.";
        return false;
    }
    const tinygltf::Buffer& buffer = model.buffers[static_cast<size_t>(view.buffer)];

    const size_t comp_size = sizeof(float);
    const size_t elem_size = static_cast<size_t>(expected_components) * comp_size;
    const size_t stride = accessor.ByteStride(view) > 0 ? static_cast<size_t>(accessor.ByteStride(view)) : elem_size;
    const size_t base_offset = static_cast<size_t>(view.byteOffset) + static_cast<size_t>(accessor.byteOffset);
    if (base_offset >= buffer.data.size()) {
        out_error = "Attribute data offset out of range.";
        return false;
    }

    out_values.resize(static_cast<size_t>(accessor.count) * static_cast<size_t>(expected_components));
    for (size_t i = 0; i < static_cast<size_t>(accessor.count); ++i) {
        const size_t src = base_offset + i * stride;
        const size_t dst = i * static_cast<size_t>(expected_components);
        if (src + elem_size > buffer.data.size()) {
            out_error = "Attribute data out of range.";
            return false;
        }
        float f[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        std::memcpy(f, &buffer.data[src], elem_size);
        for (int c = 0; c < expected_components; ++c) {
            out_values[dst + static_cast<size_t>(c)] = f[c];
        }
    }
    return true;
}

} // namespace

bool LoadGltfModel(
    const std::filesystem::path& model_path,
    LoadedModel& out_model,
    std::string& out_error) {
    out_model.meshes.clear();
    out_error.clear();

    if (!std::filesystem::exists(model_path)) {
        out_error = "glTF file does not exist: " + model_path.string();
        return false;
    }

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string warn;
    std::string err;

    const std::string ext = model_path.extension().string();
    bool ok = false;
    if (ext == ".glb" || ext == ".GLB")
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, model_path.string());
    else
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, model_path.string());

    if (!ok) {
        out_error = err.empty() ? ("Failed to load glTF: " + model_path.string()) : err;
        return false;
    }

    for (const tinygltf::Mesh& src_mesh : model.meshes) {
        for (const tinygltf::Primitive& prim : src_mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES)
                continue;

            auto pos_it = prim.attributes.find("POSITION");
            if (pos_it == prim.attributes.end())
                continue;

            const int pos_accessor_index = pos_it->second;
            if (pos_accessor_index < 0 || pos_accessor_index >= static_cast<int>(model.accessors.size()))
                continue;
            const tinygltf::Accessor& pos_accessor = model.accessors[static_cast<size_t>(pos_accessor_index)];

            std::vector<float> positions;
            std::string read_error;
            if (!ReadFloatVec(model, pos_accessor, TINYGLTF_TYPE_VEC3, 3, positions, read_error))
                continue;

            std::vector<float> normals;
            auto normal_it = prim.attributes.find("NORMAL");
            if (normal_it != prim.attributes.end()) {
                const int normal_accessor_index = normal_it->second;
                if (normal_accessor_index >= 0 && normal_accessor_index < static_cast<int>(model.accessors.size())) {
                    const tinygltf::Accessor& normal_accessor = model.accessors[static_cast<size_t>(normal_accessor_index)];
                    ReadFloatVec(model, normal_accessor, TINYGLTF_TYPE_VEC3, 3, normals, read_error);
                }
            }

            std::vector<float> texcoords;
            auto uv_it = prim.attributes.find("TEXCOORD_0");
            if (uv_it != prim.attributes.end()) {
                const int uv_accessor_index = uv_it->second;
                if (uv_accessor_index >= 0 && uv_accessor_index < static_cast<int>(model.accessors.size())) {
                    const tinygltf::Accessor& uv_accessor = model.accessors[static_cast<size_t>(uv_accessor_index)];
                    ReadFloatVec(model, uv_accessor, TINYGLTF_TYPE_VEC2, 2, texcoords, read_error);
                }
            }

            std::vector<int> indices;
            if (prim.indices >= 0 && prim.indices < static_cast<int>(model.accessors.size())) {
                const tinygltf::Accessor& index_accessor = model.accessors[static_cast<size_t>(prim.indices)];
                if (!ReadIndices(model, index_accessor, indices, read_error))
                    continue;
            }
            else {
                const size_t vertex_count = static_cast<size_t>(pos_accessor.count);
                indices.resize(vertex_count);
                for (size_t i = 0; i < vertex_count; ++i) {
                    indices[i] = static_cast<int>(i);
                }
            }

            LoadedMesh dst_mesh;
            dst_mesh.name = src_mesh.name;
            if (dst_mesh.name.empty())
                dst_mesh.name = "gltf_mesh";

            dst_mesh.vertices.reserve(indices.size());
            const size_t vertex_count = static_cast<size_t>(pos_accessor.count);
            for (int idx : indices) {
                if (idx < 0 || static_cast<size_t>(idx) >= vertex_count)
                    continue;

                LoadedVertex v;
                const size_t p = static_cast<size_t>(idx) * 3u;
                v.px = positions[p + 0];
                v.py = positions[p + 1];
                v.pz = positions[p + 2];

                if (normals.size() >= (static_cast<size_t>(idx) + 1u) * 3u) {
                    const size_t n = static_cast<size_t>(idx) * 3u;
                    v.nx = normals[n + 0];
                    v.ny = normals[n + 1];
                    v.nz = normals[n + 2];
                }
                if (texcoords.size() >= (static_cast<size_t>(idx) + 1u) * 2u) {
                    const size_t t = static_cast<size_t>(idx) * 2u;
                    v.u = texcoords[t + 0];
                    v.v = texcoords[t + 1];
                }

                dst_mesh.vertices.push_back(v);
            }

            if (!dst_mesh.vertices.empty())
                out_model.meshes.push_back(std::move(dst_mesh));
        }
    }

    if (out_model.meshes.empty()) {
        out_error = "No triangle primitives found in glTF: " + model_path.string();
        return false;
    }

    return true;
}

} // namespace loaders::detail
