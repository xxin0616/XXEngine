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
};

struct ShaderEffectPaths {
    std::filesystem::path vertex_path;
    std::filesystem::path fragment_path;
};

using PFNGLCREATESHADERPROC = GLuint (*)(GLenum type);
using PFNGLSHADERSOURCEPROC = void (*)(GLuint shader, GLsizei count, const char* const* string, const GLint* length);
using PFNGLCOMPILESHADERPROC = void (*)(GLuint shader);
using PFNGLGETSHADERIVPROC = void (*)(GLuint shader, GLenum pname, GLint* params);
using PFNGLGETSHADERINFOLOGPROC = void (*)(GLuint shader, GLsizei maxLength, GLsizei* length, char* infoLog);
using PFNGLDELETESHADERPROC = void (*)(GLuint shader);
using PFNGLCREATEPROGRAMPROC = GLuint (*)();
using PFNGLATTACHSHADERPROC = void (*)(GLuint program, GLuint shader);
using PFNGLLINKPROGRAMPROC = void (*)(GLuint program);
using PFNGLGETPROGRAMIVPROC = void (*)(GLuint program, GLenum pname, GLint* params);
using PFNGLGETPROGRAMINFOLOGPROC = void (*)(GLuint program, GLsizei maxLength, GLsizei* length, char* infoLog);
using PFNGLUSEPROGRAMPROC = void (*)(GLuint program);
using PFNGLDELETEPROGRAMPROC = void (*)(GLuint program);
using PFNGLGETUNIFORMLOCATIONPROC = GLint (*)(GLuint program, const char* name);
using PFNGLUNIFORM1IPROC = void (*)(GLint location, GLint v0);
using PFNGLUNIFORM1FPROC = void (*)(GLint location, GLfloat v0);
using PFNGLUNIFORM3FPROC = void (*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
using PFNGLACTIVETEXTUREPROC = void (*)(GLenum texture);

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif
#ifndef GL_TEXTURE3
#define GL_TEXTURE3 0x84C3
#endif
#ifndef GL_TEXTURE4
#define GL_TEXTURE4 0x84C4
#endif

struct GlFns {
    bool loaded = false;
    PFNGLCREATESHADERPROC CreateShader = nullptr;
    PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC CompileShader = nullptr;
    PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC DeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
    PFNGLATTACHSHADERPROC AttachShader = nullptr;
    PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
    PFNGLUSEPROGRAMPROC UseProgram = nullptr;
    PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
    PFNGLUNIFORM1IPROC Uniform1i = nullptr;
    PFNGLUNIFORM1FPROC Uniform1f = nullptr;
    PFNGLUNIFORM3FPROC Uniform3f = nullptr;
    PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
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
bool g_auto_rotate_model = false;
bool g_camera_dragging = false;
bool g_model_rotating = false;

GlFns g_gl;
std::unordered_map<int, GLuint> g_shader_program_by_effect;

void ResetCameraAndModelPose() {
    g_camera_offset_x = 0.0f;
    g_camera_offset_y = 0.0f;
    g_model_offset_x = 0.0f;
    g_model_offset_y = 0.0f;
    g_model_rotate_x_deg = 0.0f;
    g_model_rotate_y_deg = 0.0f;
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
        g_camera_dragging = true;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        g_model_rotating = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        g_camera_dragging = false;
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
        g_model_rotate_x_deg = std::clamp(g_model_rotate_x_deg, -89.0f, 89.0f);
        if (g_model_rotate_y_deg > 360.0f)
            g_model_rotate_y_deg -= 360.0f;
        else if (g_model_rotate_y_deg < -360.0f)
            g_model_rotate_y_deg += 360.0f;
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

    const bool flip_y_axis = (format == loaders::ModelFormat::Obj);
    for (const auto& mesh : model.meshes) {
        for (const auto& in_v : mesh.vertices) {
            Vertex out_v;
            out_v.x = in_v.px;
            out_v.y = flip_y_axis ? -in_v.py : in_v.py;
            out_v.z = in_v.pz;
            out_v.nx = in_v.nx;
            out_v.ny = flip_y_axis ? -in_v.ny : in_v.ny;
            out_v.nz = in_v.nz;
            out_v.u = in_v.u;
            out_v.v = in_v.v;
            g_loaded_mesh.vertices.push_back(out_v);
        }
    }

    NormalizeMesh(g_loaded_mesh.vertices);

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

bool LoadGlFunctions() {
    if (g_gl.loaded)
        return true;

    auto load = [](const char* name) -> void* {
        return reinterpret_cast<void*>(glfwGetProcAddress(name));
    };

    g_gl.CreateShader = reinterpret_cast<PFNGLCREATESHADERPROC>(load("glCreateShader"));
    g_gl.ShaderSource = reinterpret_cast<PFNGLSHADERSOURCEPROC>(load("glShaderSource"));
    g_gl.CompileShader = reinterpret_cast<PFNGLCOMPILESHADERPROC>(load("glCompileShader"));
    g_gl.GetShaderiv = reinterpret_cast<PFNGLGETSHADERIVPROC>(load("glGetShaderiv"));
    g_gl.GetShaderInfoLog = reinterpret_cast<PFNGLGETSHADERINFOLOGPROC>(load("glGetShaderInfoLog"));
    g_gl.DeleteShader = reinterpret_cast<PFNGLDELETESHADERPROC>(load("glDeleteShader"));
    g_gl.CreateProgram = reinterpret_cast<PFNGLCREATEPROGRAMPROC>(load("glCreateProgram"));
    g_gl.AttachShader = reinterpret_cast<PFNGLATTACHSHADERPROC>(load("glAttachShader"));
    g_gl.LinkProgram = reinterpret_cast<PFNGLLINKPROGRAMPROC>(load("glLinkProgram"));
    g_gl.GetProgramiv = reinterpret_cast<PFNGLGETPROGRAMIVPROC>(load("glGetProgramiv"));
    g_gl.GetProgramInfoLog = reinterpret_cast<PFNGLGETPROGRAMINFOLOGPROC>(load("glGetProgramInfoLog"));
    g_gl.UseProgram = reinterpret_cast<PFNGLUSEPROGRAMPROC>(load("glUseProgram"));
    g_gl.DeleteProgram = reinterpret_cast<PFNGLDELETEPROGRAMPROC>(load("glDeleteProgram"));
    g_gl.GetUniformLocation = reinterpret_cast<PFNGLGETUNIFORMLOCATIONPROC>(load("glGetUniformLocation"));
    g_gl.Uniform1i = reinterpret_cast<PFNGLUNIFORM1IPROC>(load("glUniform1i"));
    g_gl.Uniform1f = reinterpret_cast<PFNGLUNIFORM1FPROC>(load("glUniform1f"));
    g_gl.Uniform3f = reinterpret_cast<PFNGLUNIFORM3FPROC>(load("glUniform3f"));
    g_gl.ActiveTexture = reinterpret_cast<PFNGLACTIVETEXTUREPROC>(load("glActiveTexture"));

    const bool ok = g_gl.CreateShader && g_gl.ShaderSource && g_gl.CompileShader &&
        g_gl.GetShaderiv && g_gl.GetShaderInfoLog && g_gl.DeleteShader && g_gl.CreateProgram &&
        g_gl.AttachShader && g_gl.LinkProgram && g_gl.GetProgramiv && g_gl.GetProgramInfoLog &&
        g_gl.UseProgram && g_gl.DeleteProgram && g_gl.GetUniformLocation && g_gl.Uniform1i &&
        g_gl.Uniform1f && g_gl.Uniform3f && g_gl.ActiveTexture;

    g_gl.loaded = ok;
    return ok;
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
    GLuint shader = g_gl.CreateShader(type);
    const char* src = source.c_str();
    g_gl.ShaderSource(shader, 1, &src, nullptr);
    g_gl.CompileShader(shader);

    GLint compiled = 0;
    g_gl.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        g_gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint BuildShaderProgram(const ShaderEffectPaths& paths) {
    if (!LoadGlFunctions())
        return 0;

    const std::string vs = ReadTextFile(paths.vertex_path);
    const std::string fs = ReadTextFile(paths.fragment_path);
    if (vs.empty() || fs.empty())
        return 0;

    const GLuint vert = CompileShader(GL_VERTEX_SHADER, vs);
    const GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fs);
    if (vert == 0 || frag == 0) {
        if (vert != 0)
            g_gl.DeleteShader(vert);
        if (frag != 0)
            g_gl.DeleteShader(frag);
        return 0;
    }

    GLuint program = g_gl.CreateProgram();
    g_gl.AttachShader(program, vert);
    g_gl.AttachShader(program, frag);
    g_gl.LinkProgram(program);
    g_gl.DeleteShader(vert);
    g_gl.DeleteShader(frag);

    GLint linked = 0;
    g_gl.GetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        g_gl.DeleteProgram(program);
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
            g_gl.UseProgram(pbr_program);

            const GLint loc_base = g_gl.GetUniformLocation(pbr_program, "uBaseColorTex");
            const GLint loc_mr = g_gl.GetUniformLocation(pbr_program, "uMetalRoughTex");
            const GLint loc_normal = g_gl.GetUniformLocation(pbr_program, "uNormalTex");
            const GLint loc_ao = g_gl.GetUniformLocation(pbr_program, "uAOTex");
            const GLint loc_emissive = g_gl.GetUniformLocation(pbr_program, "uEmissiveTex");
            const GLint loc_has_base = g_gl.GetUniformLocation(pbr_program, "uHasBaseColorTex");
            const GLint loc_has_mr = g_gl.GetUniformLocation(pbr_program, "uHasMetalRoughTex");
            const GLint loc_has_normal = g_gl.GetUniformLocation(pbr_program, "uHasNormalTex");
            const GLint loc_has_ao = g_gl.GetUniformLocation(pbr_program, "uHasAOTex");
            const GLint loc_has_emissive = g_gl.GetUniformLocation(pbr_program, "uHasEmissiveTex");
            const GLint loc_light_pos = g_gl.GetUniformLocation(pbr_program, "uLightPosView");
            const GLint loc_light_color = g_gl.GetUniformLocation(pbr_program, "uLightColor");
            const GLint loc_ambient = g_gl.GetUniformLocation(pbr_program, "uAmbientIntensity");

            if (loc_base >= 0)
                g_gl.Uniform1i(loc_base, 0);
            if (loc_mr >= 0)
                g_gl.Uniform1i(loc_mr, 1);
            if (loc_normal >= 0)
                g_gl.Uniform1i(loc_normal, 2);
            if (loc_ao >= 0)
                g_gl.Uniform1i(loc_ao, 3);
            if (loc_emissive >= 0)
                g_gl.Uniform1i(loc_emissive, 4);

            if (loc_has_base >= 0)
                g_gl.Uniform1i(loc_has_base, g_loaded_mesh.has_pbr_texture[0] ? 1 : 0);
            if (loc_has_mr >= 0)
                g_gl.Uniform1i(loc_has_mr, g_loaded_mesh.has_pbr_texture[1] ? 1 : 0);
            if (loc_has_normal >= 0)
                g_gl.Uniform1i(loc_has_normal, g_loaded_mesh.has_pbr_texture[2] ? 1 : 0);
            if (loc_has_ao >= 0)
                g_gl.Uniform1i(loc_has_ao, g_loaded_mesh.has_pbr_texture[3] ? 1 : 0);
            if (loc_has_emissive >= 0)
                g_gl.Uniform1i(loc_has_emissive, g_loaded_mesh.has_pbr_texture[4] ? 1 : 0);

            if (loc_light_pos >= 0)
                g_gl.Uniform3f(loc_light_pos, 1.8f, 2.2f, 2.5f);
            if (loc_light_color >= 0)
                g_gl.Uniform3f(loc_light_color, 12.0f, 12.0f, 12.0f);
            if (loc_ambient >= 0)
                g_gl.Uniform3f(loc_ambient, 0.04f, 0.04f, 0.04f);

            return pbr_program;
        }
    }

    if (g_shading_effect != OpenGLShadingEffect::BlinnPhong)
        return 0;

    const GLuint blinn_program = EnsureShaderProgram(OpenGLShadingEffect::BlinnPhong);
    if (blinn_program == 0)
        return 0;

    g_gl.UseProgram(blinn_program);

    const GLint loc_use_tex = g_gl.GetUniformLocation(blinn_program, "uUseTexture");
    const GLint loc_tex = g_gl.GetUniformLocation(blinn_program, "uDiffuseTex");
    const GLint loc_ambient = g_gl.GetUniformLocation(blinn_program, "uAmbientColor");
    const GLint loc_diffuse = g_gl.GetUniformLocation(blinn_program, "uDiffuseColor");
    const GLint loc_spec = g_gl.GetUniformLocation(blinn_program, "uSpecularColor");
    const GLint loc_shine = g_gl.GetUniformLocation(blinn_program, "uShininess");
    const GLint loc_light = g_gl.GetUniformLocation(blinn_program, "uLightPosView");

    if (loc_use_tex >= 0)
        g_gl.Uniform1i(loc_use_tex, use_texture ? 1 : 0);
    if (loc_tex >= 0)
        g_gl.Uniform1i(loc_tex, 0);
    if (loc_ambient >= 0)
        g_gl.Uniform3f(loc_ambient, 0.06f, 0.06f, 0.06f);
    if (loc_diffuse >= 0)
        g_gl.Uniform3f(loc_diffuse, 0.84f, 0.84f, 0.87f);
    if (loc_spec >= 0)
        g_gl.Uniform3f(loc_spec, 0.70f, 0.70f, 0.70f);
    if (loc_shine >= 0)
        g_gl.Uniform1f(loc_shine, 64.0f);
    if (loc_light >= 0)
        g_gl.Uniform3f(loc_light, 1.8f, 2.2f, 2.5f);

    return blinn_program;
}

void EndShadingProgram(GLuint program) {
    if (program != 0)
        g_gl.UseProgram(0);
}

void DrawTriangleFallback() {
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLES);
    glColor3f(0.95f, 0.20f, 0.18f);
    glVertex2f(0.50f, 0.14f);
    glColor3f(0.16f, 0.76f, 0.34f);
    glVertex2f(0.18f, 0.82f);
    glColor3f(0.12f, 0.47f, 0.95f);
    glVertex2f(0.82f, 0.82f);
    glEnd();
}

void DrawLoadedMesh(float aspect) {
    EnsureSelectedModelLoaded();
    if (g_loaded_mesh.vertices.empty())
        return;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClear(GL_DEPTH_BUFFER_BIT);

    const bool use_texture = g_loaded_mesh.has_texture;
    const bool can_bind_multi = LoadGlFunctions();
    const bool is_damaged_helmet = (g_loaded_mesh.model_id == 2);
    const bool use_pbr_path =
        (g_shading_effect == OpenGLShadingEffect::Pbr) && is_damaged_helmet && can_bind_multi;

    if (use_texture || use_pbr_path) {
        glEnable(GL_TEXTURE_2D);
        if (can_bind_multi) {
            g_gl.ActiveTexture(GL_TEXTURE0);
        }
        glBindTexture(GL_TEXTURE_2D, g_loaded_mesh.texture_id);

        if (use_pbr_path && can_bind_multi) {
            for (int i = 0; i < (int)g_loaded_mesh.pbr_texture_ids.size(); ++i) {
                g_gl.ActiveTexture(GL_TEXTURE0 + i);
                const GLuint tex_id = g_loaded_mesh.has_pbr_texture[(size_t)i]
                    ? g_loaded_mesh.pbr_texture_ids[(size_t)i]
                    : 0;
                glBindTexture(GL_TEXTURE_2D, tex_id);
            }
            g_gl.ActiveTexture(GL_TEXTURE0);
        }
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_COLOR_MATERIAL);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    const float fov_deg = 45.0f;
    if (aspect < 1e-5f)
        aspect = 1.0f;
    const float z_near = 0.1f;
    const float z_far = 100.0f;
    const float ymax = z_near * std::tan(fov_deg * 3.14159265f / 360.0f);
    const float xmax = ymax * aspect;
    glFrustum(-xmax, xmax, -ymax, ymax, z_near, z_far);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(g_camera_offset_x, g_camera_offset_y, -3.2f);
    glRotatef(-15.0f, 1.0f, 0.0f, 0.0f);
    glTranslatef(g_model_offset_x, g_model_offset_y, 0.0f);
    glRotatef(g_model_rotate_x_deg, 1.0f, 0.0f, 0.0f);
    glRotatef(g_model_rotate_y_deg, 0.0f, 1.0f, 0.0f);

    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    RasterizerFeature::GetModelOptionOpenGLRotationDeg(g_render_model_index, rx, ry, rz);
    if (std::abs(rx) > 1e-6f)
        glRotatef(rx, 1.0f, 0.0f, 0.0f);
    if (std::abs(ry) > 1e-6f)
        glRotatef(ry, 0.0f, 1.0f, 0.0f);
    if (std::abs(rz) > 1e-6f)
        glRotatef(rz, 0.0f, 0.0f, 1.0f);

    const GLuint program = BeginShadingProgram(use_texture);

    glBegin(GL_TRIANGLES);
    for (const auto& v : g_loaded_mesh.vertices) {
        glNormal3f(v.nx, v.ny, v.nz);
        if (use_texture)
            glTexCoord2f(v.u, v.v);
        glVertex3f(v.x, v.y, v.z);
    }
    glEnd();

    EndShadingProgram(program);

    if (use_pbr_path && can_bind_multi) {
        for (int i = 0; i < (int)g_loaded_mesh.pbr_texture_ids.size(); ++i) {
            g_gl.ActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        g_gl.ActiveTexture(GL_TEXTURE0);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_TEXTURE_2D);
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
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0, 1.0, 1.0, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        DrawTriangleFallback();
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
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

    const char* reset_label = "Reset Camera/Model";
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
