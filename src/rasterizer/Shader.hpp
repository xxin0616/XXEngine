#ifndef RASTERIZER_SHADER_H
#define RASTERIZER_SHADER_H

#include <Eigen/Eigen>
#include "Texture.hpp"

// VertexShader out 的所有变量
// FragmentShader in 的所有变量
struct fragment_shader_payload {
	fragment_shader_payload() {
	}

	fragment_shader_payload(const Eigen::Vector3f& col, const Eigen::Vector3f& nor, const Eigen::Vector2f& tc, std::vector<Texture*> tex):
		color(col), normal(nor), tex_coords(tc), textures(tex) {
	}

	Eigen::Vector3f view_pos;
	Eigen::Vector3f color;//【0-1】
	Eigen::Vector3f normal;
	Eigen::Vector2f tex_coords;
	std::vector<Texture*> textures;
};

struct vertex_shader_payload {
	Eigen::Vector3f position;
};

#endif