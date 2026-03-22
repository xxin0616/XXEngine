#include "Texture.hpp"

#include "../../lib/stb_image/stb_image.h"

#include <algorithm>
#include <cmath>

Texture::Texture(const std::string& name)
{
	int channels = 0;
	unsigned char* data = stbi_load(name.c_str(), &width, &height, &channels, 3);
	if (data && width > 0 && height > 0)
	{
		image_data.assign(data, data + (size_t)width * (size_t)height * 3u);
		stbi_image_free(data);
		return;
	}

	if (data)
		stbi_image_free(data);

	// Fallback checkerboard texture.
	width = 256;
	height = 256;
	image_data.resize((size_t)width * (size_t)height * 3u, 0u);
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			const bool odd = ((x / 32) + (y / 32)) % 2 == 1;
			const unsigned char c = odd ? 220 : 40;
			const size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 3u;
			image_data[idx + 0] = c;
			image_data[idx + 1] = c;
			image_data[idx + 2] = c;
		}
	}
}

Vector2f Texture::applyWrapMode(float u, float v)
{
	switch (wrappingMode)
	{
	case WrappingMode::REPEAT:
		u = u - std::floor(u);
		v = v - std::floor(v);
		break;
	case WrappingMode::MIRRORED_REPEAT:
		u = std::abs(std::fmod(u, 2.0f));
		if (u > 1.0f)
			u = 2.0f - u;
		v = std::abs(std::fmod(v, 2.0f));
		if (v > 1.0f)
			v = 2.0f - v;
		break;
	case WrappingMode::CLAMP_TO_BORDER:
	case WrappingMode::CLAMP_TO_EDGE:
	default:
		u = std::clamp(u, 0.0f, 1.0f);
		v = std::clamp(v, 0.0f, 1.0f);
		break;
	}
	return { u, v };
}

Vector3f Texture::getColor(float u, float v, bool under1)
{
	if (width <= 0 || height <= 0 || image_data.empty())
		return under1 ? Vector3f(0, 0, 0) : Vector3f(0, 0, 0);

	if (wrappingMode == WrappingMode::CLAMP_TO_BORDER && (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f))
		return border_color;

	auto uv = applyWrapMode(u, v);
	u = uv.x();
	v = uv.y();

	const int x = std::clamp((int)std::floor(u * (float)(width - 1)), 0, width - 1);
	const int y = std::clamp((int)std::floor((1.0f - v) * (float)(height - 1)), 0, height - 1);
	const size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 3u;

	if (under1)
		return Vector3f(image_data[idx + 0] / 255.0f, image_data[idx + 1] / 255.0f, image_data[idx + 2] / 255.0f);
	return Vector3f((float)image_data[idx + 0], (float)image_data[idx + 1], (float)image_data[idx + 2]);
}

Vector3f Texture::getColor(Vector2f uv, bool under1)
{
	return getColor(uv.x(), uv.y(), under1);
}

Vector3f Texture::getColorBilinear(Vector2f uv)
{
	if (width <= 0 || height <= 0 || image_data.empty())
		return Vector3f(0, 0, 0);

	float u = std::clamp(uv.x(), 0.0f, 1.0f) * (float)(width - 1);
	float v = (1.0f - std::clamp(uv.y(), 0.0f, 1.0f)) * (float)(height - 1);

	const int x0 = std::clamp((int)std::floor(u), 0, width - 1);
	const int x1 = std::clamp((int)std::ceil(u), 0, width - 1);
	const int y0 = std::clamp((int)std::floor(v), 0, height - 1);
	const int y1 = std::clamp((int)std::ceil(v), 0, height - 1);

	const float tx = u - (float)x0;
	const float ty = v - (float)y0;

	auto sample = [&](int x, int y) {
		const size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 3u;
		return Vector3f(image_data[idx + 0], image_data[idx + 1], image_data[idx + 2]);
	};

	const Vector3f c00 = sample(x0, y0);
	const Vector3f c10 = sample(x1, y0);
	const Vector3f c01 = sample(x0, y1);
	const Vector3f c11 = sample(x1, y1);

	const Vector3f c0 = c00 * (1.0f - tx) + c10 * tx;
	const Vector3f c1 = c01 * (1.0f - tx) + c11 * tx;
	const Vector3f c = c0 * (1.0f - ty) + c1 * ty;
	return c / 255.0f;
}
