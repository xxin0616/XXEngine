#ifndef RASTERIZER_TRIANGLE_H
#define RASTERIZER_TRIANGLE_H

#include <Eigen/Eigen>
#include "Config.hpp"

using namespace Eigen;
class Triangle {
public:
	Vector3f v[3];
	Vector3f w;
	Vector3f color[3];
	Vector2f tex_coords[3];
	Vector3f normal[3];

	Triangle();
	Vector3f a() const { return v[0]; }
	Vector3f b() const { return v[1]; }
	Vector3f c() const { return v[2]; }

	void setVertex(int ind, Vector3f ver);
	void setNormal(int inx, Vector3f n);
	void setColor(int ind, float r, float g, float b);
	void setColor(int ind, Vector3f color);
	void setTexCoord(int ind, float s, float t);
	void setTexCoord(int ind, Vector2f tc);
	void setClipW(int ind, float clipw) { w[ind] = clipw; }
	std::array<Vector4f, 3> toVector4() const;

	Vector3f getVertex(int index) const;
	Vector3f getColor(float x, float y) const;
	float getClipW(int ind) const { return w[ind]; }
	const Vector3f& getClipW() const;
	Vector2f getTexCoord(int ind);
};

#endif