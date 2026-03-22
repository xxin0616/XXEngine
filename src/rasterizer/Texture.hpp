#ifndef RASTERIZER_TEXTURE_H
#define RASTERIZER_TEXTURE_H

#include <Eigen/Eigen>
#include <string>
#include <vector>

using namespace Eigen;

enum WrappingMode {
	CLAMP_TO_EDGE = 0,
	REPEAT = 1,
	MIRRORED_REPEAT = 2,
	CLAMP_TO_BORDER = 3,
};

class Texture {
private:
	std::vector<unsigned char> image_data;

public:
	int width = 0;
	int height = 0;
	WrappingMode wrappingMode = WrappingMode::CLAMP_TO_EDGE;
	Vector3f border_color = { 0, 0, 0 };

	Texture() = default;
	explicit Texture(const std::string& name);
	Vector3f getColor(float u, float v, bool under1 = true);
	Vector3f getColor(Vector2f uv, bool under1 = true);

	Vector2f applyWrapMode(float u, float v);
	Vector3f getColorBilinear(Vector2f uv);
};

#endif
