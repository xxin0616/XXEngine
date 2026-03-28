#include "OpenGLFeature.h"

#include "../loaders/ModelLoader.h"
#include "../rasterizer/RasterizerFeature.h"
#include "../../lib/stb_image/stb_image.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct Vertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

struct MeshData {
    bool ready = false;
    int model_index = -1;
    int model_id = -1;
    std::vector<Vertex> vertices;
    GLuint texture_id = 0;
    bool has_texture = false;
    std::array<GLuint, 5> pbr_texture_ids{ 0, 0, 0, 0, 0 };
    std::array<bool, 5> has_pbr_texture{ false, false, false, false, false };
    GLuint vbo = 0;
    GLuint vao = 0;
};

struct Mat4 {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    Mat4() {}

    static Mat4 Identity() { return Mat4(); }

    static Mat4 Frustum(float left, float right, float bottom, float top, float zNear, float zFar) {
        Mat4 res;
        for (int i=0; i<16; ++i) res.m[i] = 0;
        res.m[0] = (2.0f * zNear) / (right - left);
        res.m[5] = (2.0f * zNear) / (top - bottom);
        res.m[8] = (right + left) / (right - left);
        res.m[9] = (top + bottom) / (top - bottom);
        res.m[10] = -(zFar + zNear) / (zFar - zNear);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
        return res;
    }

    static Mat4 Translation(float x, float y, float z) {
        Mat4 res;
        res.m[12] = x; res.m[13] = y; res.m[14] = z;
        return res;
    }

    static Mat4 RotationX(float angle_deg) {
        Mat4 res;
        float c = std::cos(angle_deg * 3.14159265f / 180.0f);
        float s = std::sin(angle_deg * 3.14159265f / 180.0f);
        res.m[5] = c; res.m[6] = s;
        res.m[9] = -s; res.m[10] = c;
        return res;
    }

    static Mat4 RotationY(float angle_deg) {
        Mat4 res;
        float c = std::cos(angle_deg * 3.14159265f / 180.0f);
        float s = std::sin(angle_deg * 3.14159265f / 180.0f);
        res.m[0] = c; res.m[2] = -s;
        res.m[8] = s; res.m[10] = c;
        return res;
    }

    static Mat4 RotationZ(float angle_deg) {
        Mat4 res;
        float c = std::cos(angle_deg * 3.14159265f / 180.0f);
        float s = std::sin(angle_deg * 3.14159265f / 180.0f);
        res.m[0] = c; res.m[1] = s;
        res.m[4] = -s; res.m[5] = c;
        return res;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 res;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                res.m[c * 4 + r] =
                    m[0 * 4 + r] * o.m[c * 4 + 0] +
                    m[1 * 4 + r] * o.m[c * 4 + 1] +
                    m[2 * 4 + r] * o.m[c * 4 + 2] +
                    m[3 * 4 + r] * o.m[c * 4 + 3];
            }
        }
        return res;
    }
};

struct Mat3 {
    float m[9] = {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
    };
    static Mat3 FromMat4(const Mat4& mat) {
        Mat3 res;
        res.m[0] = mat.m[0]; res.m[1] = mat.m[1]; res.m[2] = mat.m[2];
        res.m[3] = mat.m[4]; res.m[4] = mat.m[5]; res.m[5] = mat.m[6];
        res.m[6] = mat.m[8]; res.m[7] = mat.m[9]; res.m[8] = mat.m[10];
        return res;
    }
};

struct ShaderEffectPaths {
    std::filesystem::path vertex_path;
    std::filesystem::path fragment_path;
};

MeshData g_loaded_mesh;
int g_render_model_index = 0;
OpenGLShadingEffect g_shading_effect = OpenGLShadingEffect::BlinnPhong;
float g_camera_offset_x = 0.0f;
float g_camera_offset_y = 0.0f;
float g_model_offset_x = 0.0f;
float g_model_offset_y = 0.0f;
float g_model_rotate_x_deg = 0.0f;
float g_model_rotate_y_deg = 0.0f;
float g_model_rotate_z_deg = 0.0f;
bool g_auto_rotate_model = false;
bool g_camera_dragging = false;
bool g_model_rotating = false;
bool g_light_dragging = false;
bool g_move_light_with_lmb = false;
float g_light_pos_x = 1.8f;
float g_light_pos_y = 2.2f;
float g_light_pos_z = 2.5f;

std::unordered_map<int, GLuint> g_shader_program_by_effect;

void ResetCameraAndModelPose() {
    g_camera_offset_x = 0.0f;
    g_camera_offset_y = 0.0f;
    g_model_offset_x = 0.0f;
    g_model_offset_y = 0.0f;
    g_model_rotate_x_deg = 0.0f;
    g_model_rotate_y_deg = 0.0f;
    g_model_rotate_z_deg = 0.0f;
    g_light_pos_x = 1.8f;
    g_light_pos_y = 2.2f;
    g_light_pos_z = 2.5f;
}

void HandleInput(bool hovered) {
    ImGuiIO& io = ImGui::GetIO();

    if (g_auto_rotate_model) {
        const float auto_rotate_speed_deg = 35.0f;
        g_model_rotate_y_deg += auto_rotate_speed_deg * std::max(io.DeltaTime, 0.001f);
        if (g_model_rotate_y_deg > 360.0f)
            g_model_rotate_y_deg -= 360.0f;
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        g_move_light_with_lmb ? (g_light_dragging = true) : (g_camera_dragging = true);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        g_model_rotating = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        g_camera_dragging = false;
        g_light_dragging = false;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
        g_model_rotating = false;

    if (g_camera_dragging) {
        const float mouse_pan_sensitivity = 0.0045f;
        g_camera_offset_x += io.MouseDelta.x * mouse_pan_sensitivity;
        g_camera_offset_y -= io.MouseDelta.y * mouse_pan_sensitivity;
        g_camera_offset_x = std::clamp(g_camera_offset_x, -4.0f, 4.0f);
        g_camera_offset_y = std::clamp(g_camera_offset_y, -4.0f, 4.0f);
    }

    if (g_model_rotating) {
        const float rotate_sensitivity = 0.30f;
        g_model_rotate_y_deg += io.MouseDelta.x * rotate_sensitivity;
        g_model_rotate_x_deg += io.MouseDelta.y * rotate_sensitivity;
        g_model_rotate_x_deg = std::clamp(g_model_rotate_x_deg, -89.0f, 89.0f);
        if (g_model_rotate_y_deg > 360.0f)
            g_model_rotate_y_deg -= 360.0f;
        else if (g_model_rotate_y_deg < -360.0f)
            g_model_rotate_y_deg += 360.0f;
    }

    if (g_light_dragging) {
        const float light_drag_sensitivity = 0.012f;
        g_light_pos_x += io.MouseDelta.x * light_drag_sensitivity;
        g_light_pos_y -= io.MouseDelta.y * light_drag_sensitivity;
        g_light_pos_x = std::clamp(g_light_pos_x, -10.0f, 10.0f);
        g_light_pos_y = std::clamp(g_light_pos_y, -10.0f, 10.0f);
    }

    const bool ctrl_down = io.KeyCtrl || ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
    if (ctrl_down) {
        if (hovered) {
            // Ctrl + mouse move: rotate model without requiring mouse buttons.
            const float ctrl_mouse_rotate_sensitivity = 0.30f;
            g_model_rotate_y_deg += io.MouseDelta.x * ctrl_mouse_rotate_sensitivity;
            g_model_rotate_x_deg += io.MouseDelta.y * ctrl_mouse_rotate_sensitivity;
            g_model_rotate_x_deg = std::clamp(g_model_rotate_x_deg, -89.0f, 89.0f);
        }

        const float key_rotate_speed = 85.0f * std::max(io.DeltaTime, 0.001f);
        if (ImGui::IsKeyDown(ImGuiKey_A))
            g_model_rotate_y_deg -= key_rotate_speed;
        if (ImGui::IsKeyDown(ImGuiKey_D))
            g_model_rotate_y_deg += key_rotate_speed;
        if (ImGui::IsKeyDown(ImGuiKey_W))
            g_model_rotate_x_deg += key_rotate_speed;
        if (ImGui::IsKeyDown(ImGuiKey_S))
            g_model_rotate_x_deg -= key_rotate_speed;
        if (ImGui::IsKeyDown(ImGuiKey_Q))
            g_model_rotate_z_deg -= key_rotate_speed;
        if (ImGui::IsKeyDown(ImGuiKey_E))
            g_model_rotate_z_deg += key_rotate_speed;
        g_model_rotate_x_deg = std::clamp(g_model_rotate_x_deg, -89.0f, 89.0f);
        if (g_model_rotate_y_deg > 360.0f)
            g_model_rotate_y_deg -= 360.0f;
        else if (g_model_rotate_y_deg < -360.0f)
            g_model_rotate_y_deg += 360.0f;
        if (g_model_rotate_z_deg > 360.0f)
            g_model_rotate_z_deg -= 360.0f;
        else if (g_model_rotate_z_deg < -360.0f)
            g_model_rotate_z_deg += 360.0f;
    }
    else {
        const float move_speed = 1.6f * std::max(io.DeltaTime, 0.001f);
        if (ImGui::IsKeyDown(ImGuiKey_A))
            g_model_offset_x -= move_speed;
        if (ImGui::IsKeyDown(ImGuiKey_D))
            g_model_offset_x += move_speed;
        if (ImGui::IsKeyDown(ImGuiKey_W))
            g_model_offset_y += move_speed;
        if (ImGui::IsKeyDown(ImGuiKey_S))
            g_model_offset_y -= move_speed;
        g_model_offset_x = std::clamp(g_model_offset_x, -5.0f, 5.0f);
        g_model_offset_y = std::clamp(g_model_offset_y, -5.0f, 5.0f);
    }
}

void ReleaseLoadedTextures() {
    if (g_loaded_mesh.texture_id != 0) {
        glDeleteTextures(1, &g_loaded_mesh.texture_id);
        g_loaded_mesh.texture_id = 0;
    }
    g_loaded_mesh.has_texture = false;
    for (size_t i = 0; i < g_loaded_mesh.pbr_texture_ids.size(); ++i) {
        if (g_loaded_mesh.pbr_texture_ids[i] != 0) {
            glDeleteTextures(1, &g_loaded_mesh.pbr_texture_ids[i]);
            g_loaded_mesh.pbr_texture_ids[i] = 0;
        }
        g_loaded_mesh.has_pbr_texture[i] = false;
    }
    if (g_loaded_mesh.vbo != 0) {
        glDeleteBuffers(1, &g_loaded_mesh.vbo);
        g_loaded_mesh.vbo = 0;
    }
    if (g_loaded_mesh.vao != 0) {
        glDeleteVertexArrays(1, &g_loaded_mesh.vao);
        g_loaded_mesh.vao = 0;
    }
}

bool LoadTextureFromPath(const std::string& texture_path, GLuint& out_texture_id) {
    out_texture_id = 0;
    if (texture_path.empty())
        return false;

    const std::filesystem::path p(texture_path);
    if (!std::filesystem::exists(p))
        return false;

    stbi_set_flip_vertically_on_load(1);
    int w = 0;
    int h = 0;
    int channels = 0;
    unsigned char* data = stbi_load(texture_path.c_str(), &w, &h, &channels, 4);
    if (data == nullptr || w <= 0 || h <= 0) {
        if (data)
            stbi_image_free(data);
        return false;
    }

    glGenTextures(1, &out_texture_id);
    glBindTexture(GL_TEXTURE_2D, out_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    return true;
}

bool LoadTextureForModel(const std::string& texture_path) {
    GLuint texture_id = 0;
    if (!LoadTextureFromPath(texture_path, texture_id))
        return false;

    if (g_loaded_mesh.texture_id != 0)
        glDeleteTextures(1, &g_loaded_mesh.texture_id);
    g_loaded_mesh.texture_id = texture_id;
    g_loaded_mesh.has_texture = true;
    return true;
}

void NormalizeMesh(std::vector<Vertex>& vertices) {
    if (vertices.empty())
        return;

    float min_x = vertices[0].x;
    float max_x = vertices[0].x;
    float min_y = vertices[0].y;
    float max_y = vertices[0].y;
    float min_z = vertices[0].z;
    float max_z = vertices[0].z;
    for (const auto& v : vertices) {
        min_x = std::min(min_x, v.x);
        max_x = std::max(max_x, v.x);
        min_y = std::min(min_y, v.y);
        max_y = std::max(max_y, v.y);
        min_z = std::min(min_z, v.z);
        max_z = std::max(max_z, v.z);
    }

    const float cx = (min_x + max_x) * 0.5f;
    const float cy = (min_y + max_y) * 0.5f;
    const float cz = (min_z + max_z) * 0.5f;
    const float extent_x = max_x - min_x;
    const float extent_y = max_y - min_y;
    const float extent_z = max_z - min_z;
    const float max_extent = std::max(std::max(extent_x, extent_y), extent_z);
    const float scale = (max_extent > 1e-6f) ? (1.9f / max_extent) : 1.0f;

    for (auto& v : vertices) {
        v.x = (v.x - cx) * scale;
        v.y = (v.y - cy) * scale;
        v.z = (v.z - cz) * scale;
    }
}

void EnsureSelectedModelLoaded() {
    if (g_loaded_mesh.ready && g_loaded_mesh.model_index == g_render_model_index)
        return;

    g_loaded_mesh.ready = true;
    g_loaded_mesh.model_index = g_render_model_index;
    g_loaded_mesh.model_id = RasterizerFeature::GetModelOptionId(g_render_model_index);
    g_loaded_mesh.vertices.clear();
    ReleaseLoadedTextures();

    const std::string model_path_str = RasterizerFeature::GetModelOptionPath(g_render_model_index);
    if (model_path_str.empty())
        return;
    const std::filesystem::path model_path = std::filesystem::path(model_path_str);
    if (!std::filesystem::exists(model_path))
        return;

    const std::string loader_name = RasterizerFeature::GetModelOptionLoaderName(g_render_model_index);
    loaders::ModelFormat format = loaders::ModelFormatFromName(loader_name);
    if (format == loaders::ModelFormat::Unknown)
        format = loaders::ModelFormatFromExtension(model_path);
    if (format == loaders::ModelFormat::Unknown)
        format = loaders::ModelFormat::Obj;

    loaders::LoadedModel model;
    std::string error;
    if (!loaders::LoadModel(model_path, format, model, error))
        return;

    for (const auto& mesh : model.meshes) {
        for (const auto& in_v : mesh.vertices) {
            Vertex out_v;
            out_v.x = in_v.px;
            out_v.y = in_v.py;
            out_v.z = in_v.pz;
            out_v.nx = in_v.nx;
            out_v.ny = in_v.ny;
            out_v.nz = in_v.nz;
            out_v.u = in_v.u;
            out_v.v = in_v.v;
            g_loaded_mesh.vertices.push_back(out_v);
        }
    }

    NormalizeMesh(g_loaded_mesh.vertices);

    if (g_loaded_mesh.vao != 0) {
        glDeleteVertexArrays(1, &g_loaded_mesh.vao);
        g_loaded_mesh.vao = 0;
    }
    if (g_loaded_mesh.vbo != 0) {
        glDeleteBuffers(1, &g_loaded_mesh.vbo);
        g_loaded_mesh.vbo = 0;
    }

    glGenVertexArrays(1, &g_loaded_mesh.vao);
    glGenBuffers(1, &g_loaded_mesh.vbo);

    glBindVertexArray(g_loaded_mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_loaded_mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, g_loaded_mesh.vertices.size() * sizeof(Vertex), g_loaded_mesh.vertices.data(), GL_STATIC_DRAW);

    // Position (x, y, z)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));

    // Normal (nx, ny, nz)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));

    // UV (u, v)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));

    glBindVertexArray(0);

    const std::string texture_path = RasterizerFeature::GetModelOptionPrimaryTexturePath(g_render_model_index);
    LoadTextureForModel(texture_path);

    const int texture_count = RasterizerFeature::GetModelOptionTexturePathCount(g_render_model_index);
    const int pbr_slots = std::min(texture_count, (int)g_loaded_mesh.pbr_texture_ids.size());
    for (int i = 0; i < pbr_slots; ++i) {
        const std::string pbr_texture_path = RasterizerFeature::GetModelOptionTexturePath(g_render_model_index, i);
        GLuint texture_id = 0;
        if (LoadTextureFromPath(pbr_texture_path, texture_id)) {
            g_loaded_mesh.pbr_texture_ids[(size_t)i] = texture_id;
            g_loaded_mesh.has_pbr_texture[(size_t)i] = true;
        }
    }
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open())
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

ShaderEffectPaths ResolveShaderEffectPaths(OpenGLShadingEffect effect) {
    switch (effect) {
    case OpenGLShadingEffect::BlinnPhong:
        return ShaderEffectPaths{
            std::filesystem::path("src/OpenGL/shaders/blinnPhong/blinnphong.vert"),
            std::filesystem::path("src/OpenGL/shaders/blinnPhong/blinnphong.frag")
        };
    case OpenGLShadingEffect::Pbr:
        return ShaderEffectPaths{
            std::filesystem::path("src/OpenGL/shaders/pbr/pbr.vert"),
            std::filesystem::path("src/OpenGL/shaders/pbr/pbr.frag")
        };
    case OpenGLShadingEffect::Textured:
    default:
        return ShaderEffectPaths{};
    }
}

GLuint CompileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            std::vector<char> info_log(info_len);
            glGetShaderInfoLog(shader, info_len, nullptr, info_log.data());
            printf("Shader Compile Error (%d):\n%s\n", type, info_log.data());
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint BuildShaderProgram(const ShaderEffectPaths& paths) {
    const std::string vs = ReadTextFile(paths.vertex_path);
    const std::string fs = ReadTextFile(paths.fragment_path);
    if (vs.empty() || fs.empty())
        return 0;

    const GLuint vert = CompileShader(GL_VERTEX_SHADER, vs);
    const GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fs);
    if (vert == 0 || frag == 0) {
        if (vert != 0)
            glDeleteShader(vert);
        if (frag != 0)
            glDeleteShader(frag);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            std::vector<char> info_log(info_len);
            glGetProgramInfoLog(program, info_len, nullptr, info_log.data());
            printf("Program Link Error:\n%s\n", info_log.data());
        }
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

GLuint EnsureShaderProgram(OpenGLShadingEffect effect) {
    const int key = static_cast<int>(effect);
    const auto it = g_shader_program_by_effect.find(key);
    if (it != g_shader_program_by_effect.end())
        return it->second;

    const ShaderEffectPaths paths = ResolveShaderEffectPaths(effect);
    if (paths.vertex_path.empty() || paths.fragment_path.empty())
        return 0;

    const GLuint program = BuildShaderProgram(paths);
    if (program != 0)
        g_shader_program_by_effect[key] = program;
    return program;
}

GLuint BeginShadingProgram(bool use_texture) {
    const bool is_damaged_helmet = (g_loaded_mesh.model_id == 2);
    if (is_damaged_helmet && g_shading_effect == OpenGLShadingEffect::Pbr) {
        const GLuint pbr_program = EnsureShaderProgram(OpenGLShadingEffect::Pbr);
        if (pbr_program != 0) {
            glUseProgram(pbr_program);

            const GLint loc_base = glGetUniformLocation(pbr_program, "uBaseColorTex");
            const GLint loc_mr = glGetUniformLocation(pbr_program, "uMetalRoughTex");
            const GLint loc_normal = glGetUniformLocation(pbr_program, "uNormalTex");
            const GLint loc_ao = glGetUniformLocation(pbr_program, "uAOTex");
            const GLint loc_emissive = glGetUniformLocation(pbr_program, "uEmissiveTex");
            const GLint loc_has_base = glGetUniformLocation(pbr_program, "uHasBaseColorTex");
            const GLint loc_has_mr = glGetUniformLocation(pbr_program, "uHasMetalRoughTex");
            const GLint loc_has_normal = glGetUniformLocation(pbr_program, "uHasNormalTex");
            const GLint loc_has_ao = glGetUniformLocation(pbr_program, "uHasAOTex");
            const GLint loc_has_emissive = glGetUniformLocation(pbr_program, "uHasEmissiveTex");
            const GLint loc_light_pos = glGetUniformLocation(pbr_program, "uLightPosView");
            const GLint loc_light_color = glGetUniformLocation(pbr_program, "uLightColor");
            const GLint loc_ambient = glGetUniformLocation(pbr_program, "uAmbientIntensity");

            if (loc_base >= 0)
                glUniform1i(loc_base, 0);
            if (loc_mr >= 0)
                glUniform1i(loc_mr, 1);
            if (loc_normal >= 0)
                glUniform1i(loc_normal, 2);
            if (loc_ao >= 0)
                glUniform1i(loc_ao, 3);
            if (loc_emissive >= 0)
                glUniform1i(loc_emissive, 4);

            if (loc_has_base >= 0)
                glUniform1i(loc_has_base, g_loaded_mesh.has_pbr_texture[0] ? 1 : 0);
            if (loc_has_mr >= 0)
                glUniform1i(loc_has_mr, g_loaded_mesh.has_pbr_texture[1] ? 1 : 0);
            if (loc_has_normal >= 0)
                glUniform1i(loc_has_normal, g_loaded_mesh.has_pbr_texture[2] ? 1 : 0);
            if (loc_has_ao >= 0)
                glUniform1i(loc_has_ao, g_loaded_mesh.has_pbr_texture[3] ? 1 : 0);
            if (loc_has_emissive >= 0)
                glUniform1i(loc_has_emissive, g_loaded_mesh.has_pbr_texture[4] ? 1 : 0);

            if (loc_light_pos >= 0)
                glUniform3f(loc_light_pos, g_light_pos_x, g_light_pos_y, g_light_pos_z);
            if (loc_light_color >= 0)
                glUniform3f(loc_light_color, 12.0f, 12.0f, 12.0f);
            if (loc_ambient >= 0)
                glUniform3f(loc_ambient, 0.04f, 0.04f, 0.04f);

            return pbr_program;
        }
    }

    if (g_shading_effect != OpenGLShadingEffect::BlinnPhong)
        return 0;

    const GLuint blinn_program = EnsureShaderProgram(OpenGLShadingEffect::BlinnPhong);
    if (blinn_program == 0)
        return 0;

    glUseProgram(blinn_program);

    const GLint loc_use_tex = glGetUniformLocation(blinn_program, "uUseTexture");
    const GLint loc_tex = glGetUniformLocation(blinn_program, "uDiffuseTex");
    const GLint loc_ambient = glGetUniformLocation(blinn_program, "uAmbientColor");
    const GLint loc_diffuse = glGetUniformLocation(blinn_program, "uDiffuseColor");
    const GLint loc_spec = glGetUniformLocation(blinn_program, "uSpecularColor");
    const GLint loc_shine = glGetUniformLocation(blinn_program, "uShininess");
    const GLint loc_light = glGetUniformLocation(blinn_program, "uLightPosView");

    if (loc_use_tex >= 0)
        glUniform1i(loc_use_tex, use_texture ? 1 : 0);
    if (loc_tex >= 0)
        glUniform1i(loc_tex, 0);
    if (loc_ambient >= 0)
        glUniform3f(loc_ambient, 0.06f, 0.06f, 0.06f);
    if (loc_diffuse >= 0)
        glUniform3f(loc_diffuse, 0.84f, 0.84f, 0.87f);
    if (loc_spec >= 0)
        glUniform3f(loc_spec, 0.70f, 0.70f, 0.70f);
    if (loc_shine >= 0)
        glUniform1f(loc_shine, 64.0f);
    if (loc_light >= 0)
        glUniform3f(loc_light, g_light_pos_x, g_light_pos_y, g_light_pos_z);

    return blinn_program;
}

void EndShadingProgram(GLuint program) {
    if (program != 0)
        glUseProgram(0);
}

static GLuint g_fallback_program = 0;
static GLuint g_fallback_vao = 0;
static GLuint g_fallback_vbo = 0;

void InitFallback() {
    if (g_fallback_program != 0) return;
    const char* vs = "#version 330 core\nlayout(location=0) in vec2 aPos;\nlayout(location=1) in vec3 aColor;\nout vec3 vColor;\nvoid main(){\ngl_Position = vec4(aPos.x * 2.0 - 1.0, 1.0 - aPos.y * 2.0, 0.0, 1.0);\nvColor = aColor;\n}";
    const char* fs = "#version 330 core\nin vec3 vColor;\nout vec4 FragColor;\nvoid main(){\nFragColor = vec4(vColor, 1.0);\n}";
    
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, nullptr);
    glCompileShader(v);
    
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, nullptr);
    glCompileShader(f);
    
    g_fallback_program = glCreateProgram();
    glAttachShader(g_fallback_program, v);
    glAttachShader(g_fallback_program, f);
    glLinkProgram(g_fallback_program);
    
    float verts[] = {
        0.50f, 0.14f,  0.95f, 0.20f, 0.18f,
        0.18f, 0.82f,  0.16f, 0.76f, 0.34f,
        0.82f, 0.82f,  0.12f, 0.47f, 0.95f
    };
    
    glGenVertexArrays(1, &g_fallback_vao);
    glGenBuffers(1, &g_fallback_vbo);
    glBindVertexArray(g_fallback_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_fallback_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void DrawTriangleFallback() {
    InitFallback();
    glUseProgram(g_fallback_program);
    glBindVertexArray(g_fallback_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
}

void DrawLoadedMesh(float aspect) {
    EnsureSelectedModelLoaded();
    if (g_loaded_mesh.vertices.empty())
        return;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClear(GL_DEPTH_BUFFER_BIT);

    const bool use_texture = g_loaded_mesh.has_texture;
    const bool can_bind_multi = true;
    const bool is_damaged_helmet = (g_loaded_mesh.model_id == 2);
    const bool use_pbr_path =
        (g_shading_effect == OpenGLShadingEffect::Pbr) && is_damaged_helmet && can_bind_multi;

    if (use_texture || use_pbr_path) {
        if (can_bind_multi) {
            glActiveTexture(GL_TEXTURE0);
        }
        glBindTexture(GL_TEXTURE_2D, g_loaded_mesh.texture_id);

        if (use_pbr_path && can_bind_multi) {
            for (int i = 0; i < (int)g_loaded_mesh.pbr_texture_ids.size(); ++i) {
                glActiveTexture(GL_TEXTURE0 + i);
                const GLuint tex_id = g_loaded_mesh.has_pbr_texture[(size_t)i]
                    ? g_loaded_mesh.pbr_texture_ids[(size_t)i]
                    : 0;
                glBindTexture(GL_TEXTURE_2D, tex_id);
            }
            glActiveTexture(GL_TEXTURE0);
        }
    }

    const float fov_deg = 45.0f;
    if (aspect < 1e-5f)
        aspect = 1.0f;
    const float z_near = 0.1f;
    const float z_far = 100.0f;
    const float ymax = z_near * std::tan(fov_deg * 3.14159265f / 360.0f);
    const float xmax = ymax * aspect;

    Mat4 proj = Mat4::Frustum(-xmax, xmax, -ymax, ymax, z_near, z_far);

    Mat4 view = Mat4::Translation(g_camera_offset_x, g_camera_offset_y, -3.2f)
              * Mat4::RotationX(-15.0f);

    Mat4 model = Mat4::Translation(g_model_offset_x, g_model_offset_y, 0.0f)
               * Mat4::RotationX(g_model_rotate_x_deg)
               * Mat4::RotationY(g_model_rotate_y_deg)
               * Mat4::RotationZ(g_model_rotate_z_deg);

    float model_rx = 0.0f;
    float model_ry = 0.0f;
    float model_rz = 0.0f;
    RasterizerFeature::GetModelOptionModelRotationDeg(g_render_model_index, model_rx, model_ry, model_rz);
    if (std::abs(model_rx) > 1e-6f || std::abs(model_ry) > 1e-6f || std::abs(model_rz) > 1e-6f) {
        const Mat4 correction = Mat4::RotationZ(model_rz) * Mat4::RotationY(model_ry) * Mat4::RotationX(model_rx);
        model = model * correction;
    }

    Mat4 modelView = view * model;
    Mat3 normalMatrix = Mat3::FromMat4(modelView);

    const GLuint program = BeginShadingProgram(use_texture);
    if (program != 0) {
        GLint loc_proj = glGetUniformLocation(program, "uProjection");
        if (loc_proj >= 0) glUniformMatrix4fv(loc_proj, 1, GL_FALSE, proj.m);
        GLint loc_mv = glGetUniformLocation(program, "uModelView");
        if (loc_mv >= 0) glUniformMatrix4fv(loc_mv, 1, GL_FALSE, modelView.m);
        GLint loc_nm = glGetUniformLocation(program, "uNormalMatrix");
        if (loc_nm >= 0) glUniformMatrix3fv(loc_nm, 1, GL_FALSE, normalMatrix.m);
    }

    if (g_loaded_mesh.vao != 0) {
        glBindVertexArray(g_loaded_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(g_loaded_mesh.vertices.size()));
        glBindVertexArray(0);
    }

    EndShadingProgram(program);

    if (use_pbr_path && can_bind_multi) {
        for (int i = 0; i < (int)g_loaded_mesh.pbr_texture_ids.size(); ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE0);
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void DrawModelCallback(const ImDrawList*, const ImDrawCmd* cmd) {
    const ImVec4 clip = cmd->ClipRect;
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;

    const int viewport_x = static_cast<int>(clip.x);
    const int viewport_y = static_cast<int>(display_size.y - clip.w);
    const int viewport_w = static_cast<int>(clip.z - clip.x);
    const int viewport_h = static_cast<int>(clip.w - clip.y);
    if (viewport_w <= 0 || viewport_h <= 0)
        return;

    GLint old_viewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, old_viewport);
    const GLboolean old_scissor = glIsEnabled(GL_SCISSOR_TEST);

    glViewport(viewport_x, viewport_y, viewport_w, viewport_h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(viewport_x, viewport_y, viewport_w, viewport_h);

    EnsureSelectedModelLoaded();
    if (!g_loaded_mesh.vertices.empty()) {
        const float aspect = static_cast<float>(viewport_w) / static_cast<float>(viewport_h);
        DrawLoadedMesh(aspect);
    }
    else {
        DrawTriangleFallback();
    }

    if (old_scissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
}

} // namespace

void OpenGLFeature::RenderInImGuiChild(int model_index) {
    g_render_model_index = model_index;

    const char* model_name = RasterizerFeature::GetModelOptionName(model_index);
    const std::string title = std::string("OpenGL backend MVP: ") + (model_name ? model_name : "model");
    ImGui::TextUnformatted(title.c_str());

    const char* reset_label = "Reset";
    const char* auto_label = g_auto_rotate_model ? "Stop Auto Rotate" : "Auto Rotate";
    const ImVec2 text_size_reset = ImGui::CalcTextSize(reset_label);
    const ImVec2 text_size_auto = ImGui::CalcTextSize(auto_label);
    const ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
    const float button_height = ImGui::GetFrameHeight();
    const float reset_button_width = (text_size_reset.x + frame_padding.x * 2.0f) / 0.8f;
    const float auto_button_width = (text_size_auto.x + frame_padding.x * 2.0f) / 0.8f;

    if (ImGui::Button(reset_label, ImVec2(reset_button_width, button_height))) {
        ResetCameraAndModelPose();
    }
    ImGui::SameLine(0.0f, 14.0f);
    if (ImGui::Button(auto_label, ImVec2(auto_button_width, button_height))) {
        g_auto_rotate_model = !g_auto_rotate_model;
    }
    ImGui::SameLine(0.0f, 14.0f);
    ImGui::Checkbox("LMB Drag Moves Light", &g_move_light_with_lmb);

    ImGui::Spacing();

    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 1.0f || canvas_size.y < 1.0f)
        return;

    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(24, 28, 34, 255));

    draw_list->PushClipRect(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        true);
    draw_list->AddCallback(DrawModelCallback, nullptr);
    draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    draw_list->PopClipRect();

    ImGui::InvisibleButton(
        "##OpenGLFeatureCanvas",
        canvas_size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    const bool canvas_hovered = ImGui::IsItemHovered();
    const bool canvas_active = ImGui::IsItemActive();
    HandleInput(canvas_hovered || canvas_active);
}

void OpenGLFeature::SetShadingEffect(OpenGLShadingEffect effect) {
    g_shading_effect = effect;
}
