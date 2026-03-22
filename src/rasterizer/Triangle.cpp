#include "Triangle.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

Triangle::Triangle() {
	v[0] << 0, 0, 0;
	v[1] << 0, 0, 0;
	v[2] << 0, 0, 0;

	color[0] << 0.0, 0.0, 0.0;
	color[1] << 0.0, 0.0, 0.0;
	color[2] << 0.0, 0.0, 0.0;

	tex_coords[0] << 0.0, 0.0;
	tex_coords[1] << 0.0, 0.0;
	tex_coords[2] << 0.0, 0.0;
}

void Triangle::setVertex(int ind, Vector3f ver) {
	v[ind] = ver;
}

void Triangle::setNormal(int ind, Vector3f n) {
	normal[ind] = n;
}

void Triangle::setColor(int ind, float r, float g, float b) {
	if (r < 0.0 || r > 255.0 ||
		g < 0.0 || g > 255.0 ||
		b < 0.0 || b > 255.0) {
		return;
	}

	color[ind] = Vector3f((float)r / 255., (float)g / 255., (float)b / 255.);
	return;
}

void Triangle::setColor(int ind, Vector3f c) {
	if (c.x() < 0.0 || c.x() > 1.0 ||
		c.y() < 0.0 || c.y() > 1.0 ||
		c.z() < 0.0 || c.z() > 1.0) {
		return;
	}

	color[ind] = c;
	return;
}

void Triangle::setTexCoord(int ind, float s, float t) {
	tex_coords[ind] = Vector2f(s, t);
}

void Triangle::setTexCoord(int ind, Vector2f tc) {
	tex_coords[ind] = tc;
}

Vector2f Triangle::getTexCoord(int ind) {
	return tex_coords[ind];
}

std::array<Vector4f, 3> Triangle::toVector4() const {
	std::array<Vector4f, 3> res;
	res[0] = { v[0].x(), v[0].y(), v[0].z(), w[0] };
	res[1] = { v[1].x(), v[1].y(), v[1].z(), w[1] };
	res[2] = { v[2].x(), v[2].y(), v[2].z(), w[2] };
	return res;
}

Vector3f Triangle::getVertex(int index) const {
	return v[index];
}

Vector3f Triangle::getColor(float x, float y) const {
	// TODO：根据目标坐标和顶点的颜色，插值出坐标的颜色
	return color[0];
}

const Vector3f& Triangle::getClipW() const{
	return w;
}
