#pragma once

#include <algorithm>
#include <Eigen/Eigen>
#include "Triangle.hpp"
#include "util.hpp"
#include "Texture.hpp"
#include "Shader.hpp"
#include <optional>

using namespace Eigen;

namespace rst {
	enum class Buffers {
		Color = 1,
		Depth = 2,
		SampleColor = 3,
		SampleDepth = 4
	};

	inline Buffers operator|(Buffers a, Buffers b) {
		return Buffers((int)a | (int)b);
	}

	inline Buffers operator&(Buffers a, Buffers b) {
		return Buffers((int)a & (int)b);
	}

	enum class Primitive {
		Line,
		Triangle
	};

	struct pos_buf_id {
		int pos_id = 0;
	};

	struct ind_buf_id {
		int ind_id = 0;
	};

	struct col_buf_id {
		int col_id = 0;
	};

	class rasterizer {
	public:
		rasterizer(int w = 700, int h = 700);
		pos_buf_id load_positions(const std::vector<Eigen::Vector3f>& positions);
		ind_buf_id load_indices(const std::vector<Eigen::Vector3i>& indices);
		col_buf_id load_colors(const std::vector<Vector3f>& colors);

		void set_model(const Eigen::Matrix4f& m);
		void set_view(const Eigen::Matrix4f& v);
		void set_projection(const Eigen::Matrix4f& p);

		void set_texture(const std::vector<Texture>& tex) {
			textures.clear();
			textures.reserve(tex.size());
			for (const auto& t : tex) {
				textures.emplace_back(t);
			}
		}
		void set_vertex_shader(std::function<Vector3f(vertex_shader_payload)> vert_shader);
		void set_fragment_shader(std::function<Vector3f(fragment_shader_payload)> frag_shader);

		void set_pixel(const Vector3f& point, const Vector3f& color);
		void set_pixel(int index, const Vector3f& color);

		void clear(Buffers buffer);
		void draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer,
			col_buf_id col_buffer, Primitive type);
		void draw(std::vector<Triangle*>& TriangleList);

		std::vector<Eigen::Vector3f>& frame_buffer() { return frame_buf; }

	private:
		void draw_line(Vector3f begin, Vector3f end);
		void rasterize_wireframe(const Triangle& t);
		void rasterize_triangle(const Triangle& t);
		void rasterize_triangle(const Triangle& t, const std::array<Eigen::Vector3f, 3>& view_pos);
		void rasterize_triangle_MSAA(const Triangle& t);
		void rasterize_triangle_MSAA(const Triangle& t, const std::array<Vector3f, 3>& view_pos);
		void rasterize_triangle_SSAA(const Triangle& t);

	private:
		Matrix4f model;
		Matrix4f view;
		Matrix4f projection;

		std::map<int, std::vector<Vector3f>> pos_buf;
		std::map<int, std::vector<Vector3i>> ind_buf;
		std::map<int, std::vector<Vector3f>> col_buf;
		std::map<int, std::vector<Vector3f>> nor_buf;
		// 最里层的类是 Vector3f Vector3i，都是表示三角形嘛

		std::vector<Vector3f> frame_buf;
		std::vector<float> depth_buf;
		std::vector<Vector3f> sample_frame_buf;
		std::vector<float> sample_depth_buf;
		//MSAA:Multi-Sample Anti-Alising
		//多重采样抗锯齿：每个像素在光栅化阶段进行多次采样，对每个子采样点单独计算覆盖信息
		//但是片段着色器只在像素执行一次
		//SSAA:Super-Sample Anti-Alising
		//超级采样抗锯齿：每个子采样点都会执行一次片段着色器

		std::vector<std::optional<Texture>> textures;

		std::function<Vector3f(fragment_shader_payload)> fragment_shader;
		std::function<Vector3f(vertex_shader_payload)> vertex_shader;

		int get_index(int x, int y);
		int get_sample_index(int x, int y, int sx, int sy);
		bool resolve_pixel_color(int x, int y, Vector3f& background, Vector3f& color);
		void resolve_frame();

		int width, height;

		int next_id = 0;
		int get_next_id() { return next_id++; }
	};
}
