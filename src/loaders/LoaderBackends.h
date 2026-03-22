#pragma once

#include "ModelLoader.h"

namespace loaders::detail {

bool LoadObjModel(
    const std::filesystem::path& model_path,
    LoadedModel& out_model,
    std::string& out_error);

bool LoadGltfModel(
    const std::filesystem::path& model_path,
    LoadedModel& out_model,
    std::string& out_error);

} // namespace loaders::detail
