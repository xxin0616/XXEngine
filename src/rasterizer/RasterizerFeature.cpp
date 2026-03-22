#include "RasterizerFeature.h"

#include "../Include.h"
#include "../loaders/ModelLoader.h"
#include "../NotificationCenter.h"
#include "Config.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Triangle.hpp"
#include "rasterizer.hpp"
#include "../../lib/stb/stb_image_write.h"

#include <Eigen/Eigen>
#if defined(_WIN64)
#include <opencv2/core.hpp>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace Eigen;

namespace RasterizerFeature
{
	namespace
	{
		enum ShaderOption
		{
			TextureShader = 0,
			NormalShader = 1,
			PhongShader = 2,
			BumpShader = 3,
			DisplacementShader = 4,
			Count = 5
		};

		struct RenderParams
		{
			int w = 0;
			int h = 0;
			int shader = TextureShader;
			int model = 1;
			float angle = 0.0f;
			float scale = 1.5f;
			Vector3f eye_pos{ 0.0f, 0.0f, 5.0f };
		};

		const char* kShaderNames[ShaderOption::Count] = {
			"texture",
			"normal",
			"phong",
			"bump",
			"displacement"
		};

		struct ModelConfig
		{
			int id = 0;
			std::string name;
			std::string loader;
			std::filesystem::path model_path;
			std::vector<std::filesystem::path> texture_paths;
			std::vector<int> supported_shader_ids;
			std::array<float, 3> opengl_rotation_deg{ 0.0f, 0.0f, 0.0f };
		};

		int g_shader_option = ShaderOption::TextureShader;
		int g_model_option = 0;
		Vector3f g_eye_pos_ui{ 0.0f, 0.0f, 5.0f };
		Vector3f g_eye_pos_render{ 0.0f, 0.0f, 5.0f };
		float g_angle_ui = 0.0f;
		float g_scale_ui = 1.5f;
		std::vector<Triangle*> g_triangles;
		std::vector<Texture> g_textures;
		std::vector<ModelConfig> g_model_configs;
		std::unordered_map<int, std::string> g_shader_name_by_id;

		GLuint g_texture_id = 0;
		int g_tex_w = 0;
		int g_tex_h = 0;

		std::thread g_worker;
		std::mutex g_mutex;
		std::condition_variable g_cv;
		bool g_stop_worker = false;
		bool g_has_pending = false;
		bool g_is_rendering = false;
		RenderParams g_pending_params;

		bool g_has_completed = false;
		RenderParams g_completed_params;
		std::vector<unsigned char> g_completed_rgb;

		std::vector<unsigned char> g_display_rgb;
		int g_display_w = 0;
		int g_display_h = 0;
		int g_loaded_model_id = -1;

		int g_last_submitted_w = -1;
		int g_last_submitted_h = -1;
		int g_last_submitted_shader = -1;
		int g_last_submitted_model = -1;
		float g_last_submitted_angle = 9999.0f;
		float g_last_submitted_scale = 9999.0f;
		Vector3f g_last_submitted_eye_pos{ 9999.0f, 9999.0f, 9999.0f };

		Matrix4f get_view_matrix(const Vector3f& eye_pos)
		{
			Matrix4f view = Matrix4f::Identity();
			Matrix4f translate;
			translate << 1, 0, 0, -eye_pos[0],
				0, 1, 0, -eye_pos[1],
				0, 0, 1, -eye_pos[2],
				0, 0, 0, 1;
			view = translate * view;
			return view;
		}

		Matrix4f get_model_matrix(float angle, float scale_size = 1.0f)
		{
			Matrix4f rotation;
			const float radians = angle * static_cast<float>(MY_PI) / 180.0f;
			rotation << std::cos(radians), 0, std::sin(radians), 0,
				0, 1, 0, 0,
				-std::sin(radians), 0, std::cos(radians), 0,
				0, 0, 0, 1;

			Matrix4f scale;
			scale << scale_size, 0, 0, 0,
				0, scale_size, 0, 0,
				0, 0, scale_size, 0,
				0, 0, 0, 1;

			return rotation * scale;
		}

		Matrix4f GetModelCorrectionMatrix(int model_index)
		{
			Matrix4f correction = Matrix4f::Identity();
			if (model_index < 0 || model_index >= (int)g_model_configs.size())
				return correction;

			const std::string& name = g_model_configs[model_index].name;
			if (name == "spot" || name == "Spot")
			{
				// Spot OBJ in this project is inverted (up/down + front/back).
				// Rotate 180 deg around X to correct its default pose.
				correction << 1, 0, 0, 0,
					0, -1, 0, 0,
					0, 0, -1, 0,
					0, 0, 0, 1;
			}
			return correction;
		}

		Matrix4f get_projection_matrix(float eye_fov, float z_near, float z_far, float aspect_ratio)
		{
			Matrix4f perspective;
			const float half_fov = eye_fov / 180.0f * static_cast<float>(MY_PI) / 2.0f;
			const float cot_h_f = 1.0f / std::tan(half_fov);
			perspective << cot_h_f / aspect_ratio, 0, 0, 0,
				0, cot_h_f, 0, 0,
				0, 0, (z_near + z_far) / (z_far - z_near), -2.0f * z_near * z_far / (z_far - z_near),
				0, 0, -1, 0;
			return perspective;
		}

		Vector3f vertex_shader(const vertex_shader_payload& payload)
		{
			return payload.position;
		}

		Vector3f normal_fragment_shader(const fragment_shader_payload& payload)
		{
			return (payload.normal.head<3>().normalized() + Vector3f(1.0f, 1.0f, 1.0f)) / 2.0f;
		}

		Texture* GetPrimaryTexture(const fragment_shader_payload& payload)
		{
			if (payload.textures.empty())
				return nullptr;
			return payload.textures[0];
		}

		Vector3f texture_fragment_shader(const fragment_shader_payload& payload)
		{
			Texture* texture = GetPrimaryTexture(payload);
			if (texture == nullptr)
				return payload.color;
			return texture->getColorBilinear(payload.tex_coords);
		}

		Vector3f phong_fragment_shader(const fragment_shader_payload& payload)
		{
			Vector3f ka = Vector3f(0.005f, 0.005f, 0.005f);
			Vector3f kd = payload.color;
			Vector3f ks = Vector3f(0.7937f, 0.7937f, 0.7937f);

			auto l1 = light{ {20, 20, 20}, {500, 500, 500} };
			auto l2 = light{ {-20, 20, 0}, {500, 500, 500} };
			std::vector<light> lights = { l1, l2 };
			Vector3f amb_light_intensity{ 10, 10, 10 };

			const float shiness = 150.0f;
			Vector3f point = payload.view_pos;
			Vector3f normal = payload.normal;

			Vector3f result_color = { 0, 0, 0 };
			for (auto& l : lights)
			{
				Vector3f light_dir = (l.position - point).normalized();
				Vector3f view_dir = (g_eye_pos_render - point).normalized();
				Vector3f half_vector = (light_dir + view_dir).normalized();
				normal.normalize();

				float dist2 = (l.position - point).dot(l.position - point);
				Vector3f ambient = ka.cwiseProduct(amb_light_intensity);
				Vector3f diffuse = kd.cwiseProduct(l.intensity / dist2) * std::max(0.0f, normal.dot(light_dir));
				Vector3f specular = ks.cwiseProduct(l.intensity / dist2) * std::pow(std::max(0.0f, normal.dot(half_vector)), shiness);
				result_color += ambient + diffuse + specular;
			}
			return result_color;
		}

		Vector3f bump_fragment_shader(const fragment_shader_payload& payload)
		{
			Texture* texture = GetPrimaryTexture(payload);
			if (texture == nullptr)
				return payload.color;

			Vector3f normal = payload.normal.normalized();
			float x = normal.x();
			float y = normal.y();
			float z = normal.z();
			Vector3f t(x * y / std::sqrt(x * x + z * z), std::sqrt(x * x + z * z), z * y / std::sqrt(x * x + z * z));
			Vector3f b = normal.cross(t);

			Matrix3f tbn;
			tbn << t.x(), b.x(), x,
				t.y(), b.y(), y,
				t.z(), b.z(), z;

			float u = payload.tex_coords.x();
			float v = payload.tex_coords.y();
			float w = static_cast<float>(texture->width);
			float h = static_cast<float>(texture->height);

			float kh = 0.2f;
			float kn = 0.1f;
			float norm0 = texture->getColor(u, v, false).norm();
			float d_u = kh * kn * (texture->getColor(u + 1.0f / w, v, false).norm() - norm0);
			float d_v = kh * kn * (texture->getColor(u, v + 1.0f / h, false).norm() - norm0);

			Vector3f ln(-d_u, -d_v, 1.0f);
			Vector3f n = (tbn * ln).normalized();
			return (n + Vector3f(1, 1, 1)) * 0.5f;
		}

		Vector3f displacement_fragment_shader(const fragment_shader_payload& payload)
		{
			Texture* texture = GetPrimaryTexture(payload);
			if (texture == nullptr)
				return payload.color;

			Vector3f texture_color = texture->getColor(payload.tex_coords);
			Vector3f ka = Vector3f(0.005f, 0.005f, 0.005f);
			Vector3f kd = texture_color;
			Vector3f ks = Vector3f(0.7937f, 0.7937f, 0.7937f);

			auto l1 = light{ {20, 20, 20}, {500, 500, 500} };
			auto l2 = light{ {-20, 20, 0}, {500, 500, 500} };
			std::vector<light> lights = { l1, l2 };
			Vector3f amb_light_intensity{ 10, 10, 10 };

			float p = 150.0f;
			Vector3f point = payload.view_pos;
			Vector3f normal = payload.normal.normalized();

			float x = normal.x();
			float y = normal.y();
			float z = normal.z();
			Vector3f t(x * y / std::sqrt(x * x + z * z), std::sqrt(x * x + z * z), z * y / std::sqrt(x * x + z * z));
			Vector3f b = normal.cross(t);

			Matrix3f tbn;
			tbn << t.x(), b.x(), x,
				t.y(), b.y(), y,
				t.z(), b.z(), z;

			float u = payload.tex_coords.x();
			float v = payload.tex_coords.y();
			float w = static_cast<float>(texture->width);
			float h = static_cast<float>(texture->height);
			float kh = 0.2f;
			float kn = 0.1f;
			float norm0 = texture->getColor(u, v, false).norm();
			float d_u = kh * kn * (texture->getColor(u + 1.0f / w, v, false).norm() - norm0);
			float d_v = kh * kn * (texture->getColor(u, v + 1.0f / h, false).norm() - norm0);
			Vector3f ln(-d_u, -d_v, 1.0f);
			normal = (tbn * ln).normalized();
			point += (kn * normal * texture->getColor(u, v).norm());

			Vector3f result_color = { 0, 0, 0 };
			for (auto& l : lights)
			{
				Vector3f light_dir = (l.position - point).normalized();
				Vector3f view_dir = (g_eye_pos_render - point).normalized();
				Vector3f half_vector = (light_dir + view_dir).normalized();
				normal.normalize();
				float dist2 = (l.position - point).dot(l.position - point);
				Vector3f ambient = ka.cwiseProduct(amb_light_intensity);
				Vector3f diffuse = kd.cwiseProduct(l.intensity / dist2) * std::max(0.0f, normal.dot(light_dir));
				Vector3f specular = ks.cwiseProduct(l.intensity / dist2) * std::pow(std::max(0.0f, normal.dot(half_vector)), p);
				result_color += ambient + diffuse + specular;
			}
			return result_color;
		}

		std::function<Vector3f(fragment_shader_payload)> SelectFragmentShader(int shader_option)
		{
			switch (shader_option)
			{
			case ShaderOption::NormalShader: return normal_fragment_shader;
			case ShaderOption::PhongShader: return phong_fragment_shader;
			case ShaderOption::BumpShader: return bump_fragment_shader;
			case ShaderOption::DisplacementShader: return displacement_fragment_shader;
			case ShaderOption::TextureShader:
			default: return texture_fragment_shader;
			}
		}

		void BuildFallbackTriangles()
		{
			if (!g_triangles.empty())
				return;

			Triangle* t1 = new Triangle();
			t1->setVertex(0, Vector3f(-1.2f, -1.0f, -2.0f));
			t1->setVertex(1, Vector3f(1.2f, -1.0f, -2.0f));
			t1->setVertex(2, Vector3f(1.2f, 1.0f, -2.0f));
			t1->setNormal(0, Vector3f(0, 0, 1));
			t1->setNormal(1, Vector3f(0, 0, 1));
			t1->setNormal(2, Vector3f(0, 0, 1));
			t1->setTexCoord(0, Vector2f(0, 0));
			t1->setTexCoord(1, Vector2f(1, 0));
			t1->setTexCoord(2, Vector2f(1, 1));
			g_triangles.push_back(t1);

			Triangle* t2 = new Triangle();
			t2->setVertex(0, Vector3f(-1.2f, -1.0f, -2.0f));
			t2->setVertex(1, Vector3f(1.2f, 1.0f, -2.0f));
			t2->setVertex(2, Vector3f(-1.2f, 1.0f, -2.0f));
			t2->setNormal(0, Vector3f(0, 0, 1));
			t2->setNormal(1, Vector3f(0, 0, 1));
			t2->setNormal(2, Vector3f(0, 0, 1));
			t2->setTexCoord(0, Vector2f(0, 0));
			t2->setTexCoord(1, Vector2f(1, 1));
			t2->setTexCoord(2, Vector2f(0, 1));
			g_triangles.push_back(t2);
		}

		std::string TrimLeadingCurrentDir(std::string path)
		{
			while (path.rfind("./", 0) == 0 || path.rfind(".\\", 0) == 0)
				path = path.substr(2);
			return path;
		}

		std::filesystem::path FindModelsJsonPath()
		{
			const std::filesystem::path candidates[] = {
				std::filesystem::path("src/models/models.json"),
				std::filesystem::path("models/models.json")
			};
			for (const auto& p : candidates)
			{
				if (std::filesystem::exists(p))
					return p;
			}
			return {};
		}

		void LoadModelConfigsFromJson()
		{
			g_model_configs.clear();
			g_shader_name_by_id.clear();

			const std::filesystem::path json_path = FindModelsJsonPath();
			if (json_path.empty())
			{
				ModelConfig fallback;
				fallback.id = 1;
				fallback.name = "spot";
				fallback.loader = "obj";
				fallback.model_path = std::filesystem::path("src/models/spot/spot_triangulated_good.obj");
				fallback.texture_paths.push_back(std::filesystem::path("src/models/spot/spot_texture.png"));
				fallback.supported_shader_ids.push_back(1);
				g_shader_name_by_id[1] = "BlinnPhong";
				g_shader_name_by_id[2] = "PBR";
				g_model_configs.push_back(fallback);
				return;
			}

#if defined(_WIN64)
			cv::FileStorage fs(json_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
			if (!fs.isOpened())
			{
				ModelConfig fallback;
				fallback.id = 1;
				fallback.name = "spot";
				fallback.loader = "obj";
				fallback.model_path = std::filesystem::path("src/models/spot/spot_triangulated_good.obj");
				fallback.texture_paths.push_back(std::filesystem::path("src/models/spot/spot_texture.png"));
				fallback.supported_shader_ids.push_back(1);
				g_shader_name_by_id[1] = "BlinnPhong";
				g_shader_name_by_id[2] = "PBR";
				g_model_configs.push_back(fallback);
				return;
			}

			const std::filesystem::path base_dir = json_path.parent_path();
			cv::FileNode shader_root = fs["shader"];
			if (!shader_root.empty()) {
				const int shader_sum = (int)shader_root["sum"];
				for (int sid = 1; sid <= shader_sum; ++sid) {
					const std::string key = std::to_string(sid);
					const std::string shader_name = (std::string)shader_root[key];
					if (!shader_name.empty())
						g_shader_name_by_id[sid] = shader_name;
				}
			}
			if (g_shader_name_by_id.find(1) == g_shader_name_by_id.end())
				g_shader_name_by_id[1] = "BlinnPhong";
			if (g_shader_name_by_id.find(2) == g_shader_name_by_id.end())
				g_shader_name_by_id[2] = "PBR";

			cv::FileNode models = fs["models"];
			for (auto it = models.begin(); it != models.end(); ++it)
			{
				ModelConfig cfg;
				cfg.id = (int)(*it)["id"];
				cfg.name = (std::string)(*it)["name"];
				cfg.loader = (std::string)(*it)["loaders"];

				const std::string model_rel = (std::string)(*it)["modelpath"];
				cfg.model_path = (base_dir / TrimLeadingCurrentDir(model_rel)).lexically_normal();

				cv::FileNode textures_node = (*it)["texturespath"];
				for (auto t = textures_node.begin(); t != textures_node.end(); ++t)
				{
					const std::string tex_rel = (std::string)(*t);
					cfg.texture_paths.push_back((base_dir / TrimLeadingCurrentDir(tex_rel)).lexically_normal());
				}

				cv::FileNode shaders_node = (*it)["shaders"];
				for (auto s = shaders_node.begin(); s != shaders_node.end(); ++s)
				{
					const std::string shader_id_str = (std::string)(*s);
					try {
						const int shader_id = std::stoi(shader_id_str);
						if (shader_id > 0)
							cfg.supported_shader_ids.push_back(shader_id);
					}
					catch (...) {}
				}
				if (cfg.supported_shader_ids.empty())
				{
					cfg.supported_shader_ids.push_back(1);
					if (cfg.id == 2)
						cfg.supported_shader_ids.push_back(2);
				}

				cv::FileNode rot_node = (*it)["opengl_rotation_deg"];
				if (rot_node.isSeq() && rot_node.size() >= 3) {
					cfg.opengl_rotation_deg[0] = (float)rot_node[0];
					cfg.opengl_rotation_deg[1] = (float)rot_node[1];
					cfg.opengl_rotation_deg[2] = (float)rot_node[2];
				}

				if (!cfg.name.empty())
					g_model_configs.push_back(cfg);
			}
#else
			ModelConfig fallback;
			fallback.id = 1;
			fallback.name = "spot";
			fallback.loader = "obj";
			fallback.model_path = std::filesystem::path("src/models/spot/spot_triangulated_good.obj");
			fallback.texture_paths.push_back(std::filesystem::path("src/models/spot/spot_texture.png"));
			fallback.supported_shader_ids.push_back(1);
			g_model_configs.push_back(fallback);
#endif

			if (g_model_configs.empty())
			{
				ModelConfig fallback;
				fallback.id = 1;
				fallback.name = "spot";
				fallback.loader = "obj";
				fallback.model_path = std::filesystem::path("src/models/spot/spot_triangulated_good.obj");
				fallback.texture_paths.push_back(std::filesystem::path("src/models/spot/spot_texture.png"));
				fallback.supported_shader_ids.push_back(1);
				g_model_configs.push_back(fallback);
			}
			if (g_shader_name_by_id.empty()) {
				g_shader_name_by_id[1] = "BlinnPhong";
				g_shader_name_by_id[2] = "PBR";
			}
		}

		void ClearTriangles()
		{
			for (Triangle* t : g_triangles)
				delete t;
			g_triangles.clear();
		}

		void SelectModelAndTexture(RenderParams& param) {
			if (g_model_configs.empty())
				LoadModelConfigsFromJson();
			if (g_model_configs.empty())
			{
				BuildFallbackTriangles();
				return;
			}

			if (param.model < 0 || param.model >= (int)g_model_configs.size())
				param.model = 0;
			if (g_loaded_model_id == param.model && !g_triangles.empty())
				return;

			ClearTriangles();
			g_textures.clear();

			const ModelConfig& selected = g_model_configs[param.model];
			std::string loader = selected.loader;
			std::filesystem::path model_path = selected.model_path;
			const std::vector<std::filesystem::path>& texture_paths = selected.texture_paths;

			if (loader != "obj")
			{
				BuildFallbackTriangles();
				g_loaded_model_id = param.model;
				return;
			}

			if (!std::filesystem::exists(model_path))
			{
				const std::filesystem::path fallback_obj("src/models/spot/spot_triangulated_good.obj");
				if (std::filesystem::exists(fallback_obj))
					model_path = fallback_obj;
			}

			loaders::LoadedModel loaded_model;
			std::string load_error;
			loaders::ModelFormat model_format = loaders::ModelFormatFromName(loader);
			if (model_format == loaders::ModelFormat::Unknown)
				model_format = loaders::ModelFormatFromExtension(model_path);
			if (model_format == loaders::ModelFormat::Unknown)
				model_format = loaders::ModelFormat::Obj;

			const bool load_ok = loaders::LoadModel(model_path, model_format, loaded_model, load_error);
			if (!load_ok || loaded_model.Empty())
			{
				BuildFallbackTriangles();
				g_loaded_model_id = param.model;
				return;
			}

			for (const auto& mesh : loaded_model.meshes) {
				for (int i = 0; i + 2 < (int)mesh.vertices.size(); i += 3) {
					Triangle* t = new Triangle();
					for (int j = 0; j < 3; j++) {
						const auto& v = mesh.vertices[i + j];
						t->setVertex(j, Vector3f(v.px, v.py, v.pz));
						t->setNormal(j, Vector3f(v.nx, v.ny, v.nz));
						t->setTexCoord(j, Vector2f(v.u, v.v));
					}
					g_triangles.push_back(t);
				}
			}

			for (const auto& path : texture_paths) {
				g_textures.emplace_back(path.generic_string());
			}

			if (g_triangles.empty())
				BuildFallbackTriangles();
			g_loaded_model_id = param.model;
		}


		std::vector<unsigned char> RenderFrameCPU(const RenderParams& params)
		{
			RenderParams local_params = params;
			SelectModelAndTexture(local_params);
			if (g_triangles.empty())
				BuildFallbackTriangles();
			g_eye_pos_render = params.eye_pos;

			rst::rasterizer rasterizer(params.w, params.h);
			rasterizer.clear(rst::Buffers::Color | rst::Buffers::Depth | rst::Buffers::SampleColor | rst::Buffers::SampleDepth);

			rasterizer.set_view(get_view_matrix(params.eye_pos));
			rasterizer.set_projection(get_projection_matrix(45.0f, 0.1f, 50.0f, (float)params.w / (float)params.h));
			const Matrix4f model_matrix =
				get_model_matrix(params.angle, params.scale) * GetModelCorrectionMatrix(local_params.model);
			rasterizer.set_model(model_matrix);

			rasterizer.set_vertex_shader(vertex_shader);
			rasterizer.set_fragment_shader(SelectFragmentShader(params.shader));

			rasterizer.set_texture(g_textures);
			rasterizer.draw(g_triangles);

			std::vector<unsigned char> rgb((size_t)params.w * (size_t)params.h * 3u, 0u);
			auto& fb = rasterizer.frame_buffer();
			for (int y = 0; y < params.h; ++y)
			{
				for (int x = 0; x < params.w; ++x)
				{
					const size_t src = (size_t)y * (size_t)params.w + (size_t)x;
					const size_t dst = ((size_t)y * (size_t)params.w + (size_t)x) * 3u;
					const Vector3f& c = fb[src];
					rgb[dst + 0] = (unsigned char)std::clamp((int)std::round(c.x()), 0, 255);
					rgb[dst + 1] = (unsigned char)std::clamp((int)std::round(c.y()), 0, 255);
					rgb[dst + 2] = (unsigned char)std::clamp((int)std::round(c.z()), 0, 255);
				}
			}
			return rgb;
		}

		void WorkerLoop()
		{
			for (;;)
			{
				RenderParams params;
				{
					std::unique_lock<std::mutex> lock(g_mutex);
					g_cv.wait(lock, [] { return g_stop_worker || g_has_pending; });
					if (g_stop_worker)
						return;
					params = g_pending_params;
					g_has_pending = false;
					g_is_rendering = true;
				}

				NotificationCenter::SetPersistent("rasterizer_status", "\xE8\xBD\xAF\xE5\x85\x89\xE6\xA0\x85\xE5\x8C\x96\xE5\x99\xA8\xEF\xBC\x9A\xE6\xB8\xB2\xE6\x9F\x93\xE4\xB8\xAD...");
				std::vector<unsigned char> rgb = RenderFrameCPU(params);
				std::cout << "render ok!" << std::endl;
				NotificationCenter::ClearPersistent("rasterizer_status");
				NotificationCenter::Push("\xE8\xBD\xAF\xE5\x85\x89\xE6\xA0\x85\xE5\x8C\x96\xE5\x99\xA8\xEF\xBC\x9A\xE6\xB8\xB2\xE6\x9F\x93\xE5\xAE\x8C\xE6\x88\x90", 3.0f);

				{
					std::lock_guard<std::mutex> lock(g_mutex);
					g_completed_params = params;
					g_completed_rgb = std::move(rgb);
					g_has_completed = true;
					g_is_rendering = false;
				}
			}
		}

		void StartWorkerIfNeeded()
		{
			if (g_worker.joinable())
				return;
			g_stop_worker = false;
			g_worker = std::thread(WorkerLoop);
		}

		void EnsureTexture(int w, int h)
		{
			if (g_texture_id == 0)
				glGenTextures(1, &g_texture_id);

			if (g_tex_w == w && g_tex_h == h)
				return;

			g_tex_w = w;
			g_tex_h = h;
			std::vector<unsigned char> black((size_t)w * (size_t)h * 3u, 0u);

			glBindTexture(GL_TEXTURE_2D, g_texture_id);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, black.data());

			{
				std::lock_guard<std::mutex> lock(g_mutex);
				g_display_rgb = black;
				g_display_w = w;
				g_display_h = h;
			}

			g_last_submitted_w = -1;
			g_last_submitted_h = -1;
			g_last_submitted_shader = -1;
			g_last_submitted_model = -1;
			g_last_submitted_angle = 9999.0f;
			g_last_submitted_scale = 9999.0f;
			g_last_submitted_eye_pos = Vector3f(9999.0f, 9999.0f, 9999.0f);
		}

		void EnqueueRenderIfNeeded(int w, int h, int shader)
		{
			const bool same_eye =
				std::abs(g_eye_pos_ui.x() - g_last_submitted_eye_pos.x()) < 1e-6f &&
				std::abs(g_eye_pos_ui.y() - g_last_submitted_eye_pos.y()) < 1e-6f &&
				std::abs(g_eye_pos_ui.z() - g_last_submitted_eye_pos.z()) < 1e-6f;
			if (w == g_last_submitted_w &&
				h == g_last_submitted_h &&
				shader == g_last_submitted_shader &&
				g_model_option == g_last_submitted_model &&
				std::abs(g_angle_ui - g_last_submitted_angle) < 1e-6f &&
				std::abs(g_scale_ui - g_last_submitted_scale) < 1e-6f &&
				same_eye)
				return;

			g_last_submitted_w = w;
			g_last_submitted_h = h;
			g_last_submitted_shader = shader;
			g_last_submitted_model = g_model_option;
			g_last_submitted_angle = g_angle_ui;
			g_last_submitted_scale = g_scale_ui;
			g_last_submitted_eye_pos = g_eye_pos_ui;

			{
				std::lock_guard<std::mutex> lock(g_mutex);
				g_pending_params = RenderParams{};
				g_pending_params.w = w;
				g_pending_params.h = h;
				g_pending_params.shader = shader;
				g_pending_params.model = g_model_option;
				g_pending_params.angle = g_angle_ui;
				g_pending_params.scale = g_scale_ui;
				g_pending_params.eye_pos = g_eye_pos_ui;
				g_has_pending = true;
			}
			g_cv.notify_one();
		}

		void UploadCompletedIfAny()
		{
			RenderParams params;
			std::vector<unsigned char> rgb;
			{
				std::lock_guard<std::mutex> lock(g_mutex);
				if (!g_has_completed)
					return;
				params = g_completed_params;
				rgb = std::move(g_completed_rgb);
				g_completed_rgb.clear();
				g_has_completed = false;
			}

			if (rgb.empty())
				return;

			if (params.w == g_tex_w && params.h == g_tex_h)
			{
				glBindTexture(GL_TEXTURE_2D, g_texture_id);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, params.w, params.h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

				std::lock_guard<std::mutex> lock(g_mutex);
				g_display_rgb = std::move(rgb);
				g_display_w = params.w;
				g_display_h = params.h;
			}
		}
	}

	bool Initialize()
	{
		LoadModelConfigsFromJson();
		if (!g_model_configs.empty())
			g_model_option = std::clamp(g_model_option, 0, (int)g_model_configs.size() - 1);
		else
			g_model_option = 0;
		StartWorkerIfNeeded();
		return !g_model_configs.empty();
	}

	int GetShaderOptionCount()
	{
		return ShaderOption::Count;
	}

	const char* GetShaderOptionName(int index)
	{
		if (index < 0 || index >= ShaderOption::Count)
			return "texture";
		return kShaderNames[index];
	}

	void SetShaderOption(int index)
	{
		g_shader_option = std::clamp(index, 0, ShaderOption::Count - 1);
	}

	int GetShaderOption()
	{
		return g_shader_option;
	}

	int GetModelOptionCount()
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		return (int)g_model_configs.size();
	}

	const char* GetModelOptionName(int index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return "model";
		return g_model_configs[index].name.c_str();
	}

	std::string GetModelOptionLoaderName(int index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return "obj";
		if (g_model_configs[index].loader.empty())
			return "obj";
		return g_model_configs[index].loader;
	}

	int GetModelOptionId(int index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return -1;
		return g_model_configs[index].id;
	}

	std::string GetModelOptionPath(int index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return {};
		return g_model_configs[index].model_path.string();
	}

	std::string GetModelOptionPrimaryTexturePath(int index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return {};
		const auto& textures = g_model_configs[index].texture_paths;
		if (textures.empty())
			return {};
		return textures[0].string();
	}

	int GetModelOptionTexturePathCount(int index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return 0;
		return (int)g_model_configs[index].texture_paths.size();
	}

	std::string GetModelOptionTexturePath(int index, int texture_index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return {};
		const auto& textures = g_model_configs[index].texture_paths;
		if (texture_index < 0 || texture_index >= (int)textures.size())
			return {};
		return textures[(size_t)texture_index].string();
	}

	int GetModelOptionSupportedShaderCount(int index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return 0;
		return (int)g_model_configs[index].supported_shader_ids.size();
	}

	int GetModelOptionSupportedShaderId(int index, int shader_index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return -1;
		const auto& shader_ids = g_model_configs[index].supported_shader_ids;
		if (shader_index < 0 || shader_index >= (int)shader_ids.size())
			return -1;
		return shader_ids[(size_t)shader_index];
	}

	std::string GetShaderNameById(int shader_id)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		const auto it = g_shader_name_by_id.find(shader_id);
		if (it != g_shader_name_by_id.end())
			return it->second;
		return "Shader";
	}

	void GetModelOptionOpenGLRotationDeg(int index, float& rx, float& ry, float& rz)
	{
		rx = 0.0f;
		ry = 0.0f;
		rz = 0.0f;

		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (index < 0 || index >= (int)g_model_configs.size())
			return;

		rx = g_model_configs[index].opengl_rotation_deg[0];
		ry = g_model_configs[index].opengl_rotation_deg[1];
		rz = g_model_configs[index].opengl_rotation_deg[2];
	}

	void SetModelOption(int index)
	{
		if (g_model_configs.empty())
			LoadModelConfigsFromJson();
		if (g_model_configs.empty())
		{
			g_model_option = 0;
			return;
		}
		g_model_option = std::clamp(index, 0, (int)g_model_configs.size() - 1);
	}

	int GetModelOption()
	{
		return g_model_option;
	}

	void RenderInImGuiChild()
	{
		ImVec2 avail = ImGui::GetContentRegionAvail();
		int w = (int)std::max(64.0f, std::floor(avail.x));
		int h = (int)std::max(64.0f, std::floor(avail.y));

		EnsureTexture(w, h);
		UploadCompletedIfAny();
		ImGui::Image((ImTextureID)(intptr_t)g_texture_id, ImVec2((float)w, (float)h), ImVec2(0, 1), ImVec2(1, 0));

		const bool hovered = ImGui::IsItemHovered();
		if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
				const ImVec2 d = ImGui::GetIO().MouseDelta;
				const int dx = (int)d.x;
				const int dy = (int)d.y;
				const float mouse_sensitivity = 0.01f;
				const int max_pixel_delta = 50;
				if (std::abs(dx) <= max_pixel_delta && std::abs(dy) <= max_pixel_delta)
				{
					g_eye_pos_ui.x() += (float)dx * mouse_sensitivity;
					g_eye_pos_ui.y() -= (float)dy * mouse_sensitivity;
					g_eye_pos_ui.y() = std::clamp(g_eye_pos_ui.y(), -10.0f, 10.0f);
				}
			}

			if (hovered && ImGui::IsKeyPressed(ImGuiKey_A, true))
				g_angle_ui += 10.0f;
			if (hovered && ImGui::IsKeyPressed(ImGuiKey_D, true))
				g_angle_ui -= 10.0f;
			if (hovered && ImGui::IsKeyPressed(ImGuiKey_W, true))
				g_scale_ui += 0.1f;
			if (hovered && ImGui::IsKeyPressed(ImGuiKey_S, true))
			{
				g_scale_ui -= 0.1f;
				if (g_scale_ui < 0.1f)
					g_scale_ui = 0.1f;
			}

		// Submit render request after applying current frame input,
		// so status notifications react immediately to mouse/keyboard interaction.
		EnqueueRenderIfNeeded(w, h, g_shader_option);
	}

	bool SaveCurrentPng(const std::string& output_path)
	{
		std::vector<unsigned char> rgb;
		int w = 0;
		int h = 0;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			rgb = g_display_rgb;
			w = g_display_w;
			h = g_display_h;
		}

		if (rgb.empty() || w <= 0 || h <= 0)
			return false;
		return stbi_write_png(output_path.c_str(), w, h, 3, rgb.data(), w * 3) != 0;
	}

	bool IsRendering()
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		return g_is_rendering || g_has_pending;
	}

	void Shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			g_stop_worker = true;
		}
		g_cv.notify_all();
		if (g_worker.joinable())
			g_worker.join();

		for (Triangle* t : g_triangles)
			delete t;
		g_triangles.clear();
		g_loaded_model_id = -1;

		if (g_texture_id != 0)
		{
			glDeleteTextures(1, &g_texture_id);
			g_texture_id = 0;
		}
	}
}
