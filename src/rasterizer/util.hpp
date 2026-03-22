#include <Eigen/Eigen>
#include "Config.hpp"

using namespace Eigen;

bool insideTriangle(float x, float y, const Vector3f* _v);
std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v);
PixelCoverage checkPixelCoverage(int x, int y, const Vector3f* _v);
Vector3f interpolate(float alpha, float beta, float gamma,
	const std::array<Vector3f, 3>& source, const Vector3f& w, float w_reciprocal);
Vector3f interpolate(float alpha, float beta, float gamma,
	const Vector3f* source, const Vector3f& w, const float w_reciprocal);
Vector2f interpolate(float alpha, float beta, float gamma,
	const Vector2f* source, const Vector3f& w, const float w_reciprocal);

Vector3f reflect(const Vector3f& vec, const Vector3f& axis);
