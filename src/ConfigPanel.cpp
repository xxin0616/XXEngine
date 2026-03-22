#include "ConfigPanel.h"
#include "rasterizer/RasterizerFeature.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

int ConfigPanel::m_backendIndex = 0;

std::string ConfigPanel::GetCurrentFeatureNameEn() const {
    if (m_backendIndex == 0) {
        if (m_featureIndex == 0)
            return "bezier";
        if (m_featureIndex == 1) {
            static const char* kShaderNames[] = { "texture", "normal", "phong", "bump", "displacement" };
            const int clamped = std::clamp(m_rasterizerShaderIndex, 0, 4);
            return kShaderNames[clamped];
        }
    }
    return "feature";
}

std::string ConfigPanel::BuildOutputPath(const std::string& featureNameEn) {
    namespace fs = std::filesystem;

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);

    std::ostringstream timestamp;
    timestamp << std::put_time(&local_tm, "%m%d%H%M");

    fs::path result_dir("result");
    fs::create_directories(result_dir);

    const std::string file_name = featureNameEn + "_" + timestamp.str() + ".png";
    return (result_dir / file_name).string();
}

bool ConfigPanel::ConsumeCaptureRequest(std::string& outputPath) {
    if (!m_captureRequested)
        return false;

    outputPath = m_captureOutputPath;
    m_captureRequested = false;
    m_captureOutputPath.clear();
    return true;
}

int ConfigPanel::GetOpenGLShaderId() const {
    const int model_index = m_openglModelIndex;
    const int shader_count = RasterizerFeature::GetModelOptionSupportedShaderCount(model_index);
    if (shader_count <= 0)
        return 1;

    int selected_index = 0;
    const auto it = m_openglShaderSelectionIndexByModel.find(model_index);
    if (it != m_openglShaderSelectionIndexByModel.end())
        selected_index = it->second;
    selected_index = std::clamp(selected_index, 0, shader_count - 1);

    const int shader_id = RasterizerFeature::GetModelOptionSupportedShaderId(model_index, selected_index);
    return (shader_id > 0) ? shader_id : 1;
}

void ConfigPanel::OnRender() {
    m_featureIndex = std::clamp(m_featureIndex, 0, 1);
    m_rasterizerShaderIndex = std::clamp(m_rasterizerShaderIndex, 0, 4);

    const int model_count = RasterizerFeature::GetModelOptionCount();
    if (model_count > 0) {
        m_rasterizerModelIndex = std::clamp(m_rasterizerModelIndex, 0, model_count - 1);
        m_openglModelIndex = std::clamp(m_openglModelIndex, 0, model_count - 1);
    }
    else {
        m_rasterizerModelIndex = 0;
        m_openglModelIndex = 0;
    }

    ImGui::Text("Config");
    ImGui::Separator();
    ImGui::Indent();
    const char* backends[] = { "Raster", "OpenGL" };
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Backend:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::Combo("##Backend", &m_backendIndex, backends, IM_ARRAYSIZE(backends));

    ImGui::Indent();
    if (m_backendIndex == 0) {
        const char* features[] = { "贝塞尔曲线", "Blinn-Phong" };
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Feature:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::Combo("##Features", &m_featureIndex, features, IM_ARRAYSIZE(features));

        if (m_featureIndex == 1) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Models:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (model_count > 0) {
                std::vector<const char*> model_names;
                model_names.reserve(model_count);
                for (int i = 0; i < model_count; ++i) {
                    model_names.push_back(RasterizerFeature::GetModelOptionName(i));
                }
                ImGui::Combo("##RasterizerModel", &m_rasterizerModelIndex, model_names.data(), model_count);
            }
            else {
                ImGui::BeginDisabled();
                int dummy_model = 0;
                const char* none[] = { "No models" };
                ImGui::Combo("##RasterizerModelNone", &dummy_model, none, 1);
                ImGui::EndDisabled();
            }

            const char* shaders[] = { "texture", "normal", "phong", "bump", "displacement" };
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Shader:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::Combo("##RasterizerShader", &m_rasterizerShaderIndex, shaders, IM_ARRAYSIZE(shaders));
        }
    }
    else if (m_backendIndex == 1) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Models:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (model_count > 0) {
            std::vector<const char*> model_names;
            model_names.reserve(model_count);
            for (int i = 0; i < model_count; ++i) {
                model_names.push_back(RasterizerFeature::GetModelOptionName(i));
            }
            ImGui::Combo("##OpenGLModel", &m_openglModelIndex, model_names.data(), model_count);
        }
        else {
            ImGui::BeginDisabled();
            int dummy_model = 0;
            const char* none[] = { "No models" };
            ImGui::Combo("##OpenGLModelNone", &dummy_model, none, 1);
            ImGui::EndDisabled();
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Shader:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        const int shader_count = RasterizerFeature::GetModelOptionSupportedShaderCount(m_openglModelIndex);
        if (shader_count > 0) {
            int& selected_index = m_openglShaderSelectionIndexByModel[m_openglModelIndex];
            selected_index = std::clamp(selected_index, 0, shader_count - 1);

            std::vector<std::string> shader_names;
            std::vector<const char*> shader_name_items;
            shader_names.reserve(shader_count);
            shader_name_items.reserve(shader_count);
            for (int i = 0; i < shader_count; ++i) {
                const int shader_id = RasterizerFeature::GetModelOptionSupportedShaderId(m_openglModelIndex, i);
                std::string shader_name = RasterizerFeature::GetShaderNameById(shader_id);
                if (shader_name.empty())
                    shader_name = "Shader";
                shader_names.push_back(shader_name);
            }
            for (auto& name : shader_names)
                shader_name_items.push_back(name.c_str());
            ImGui::Combo("##OpenGLShader", &selected_index, shader_name_items.data(), shader_count);
        }
        else {
            ImGui::BeginDisabled();
            int dummy_shader = 0;
            const char* none[] = { "No shaders" };
            ImGui::Combo("##OpenGLShaderNone", &dummy_shader, none, 1);
            ImGui::EndDisabled();
        }
    }
    ImGui::Unindent();
    ImGui::Unindent();

    if (ImGui::Button("生成图片", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        m_captureOutputPath = BuildOutputPath(GetCurrentFeatureNameEn());
        m_captureRequested = true;
    }
}
