#include "util.hpp"
#include <Eigen/Eigen>

using namespace Eigen;

bool insideTriangle(float x, float y, const Vector3f* v) {
    // 灏嗙偣杞崲涓哄悜閲?
    Vector3f P(x, y, 1.0f);

    // 璁＄畻涓変釜鍙夌Н
    Vector3f edges[3] = {
        v[1] - v[0],  // AB
        v[2] - v[1],  // BC
        v[0] - v[2]   // CA
    };

    Vector3f vectors[3] = {
        P - v[0],  // AP
        P - v[1],  // BP
        P - v[2]   // CP
    };

    // 璁＄畻涓変釜鍙夌Н鐨?z 鍒嗛噺
    float cross_z[3];
    for (int i = 0; i < 3; i++) {
        cross_z[i] = edges[i].cross(vectors[i]).z();
    }

    // 妫€鏌ユ墍鏈夊弶绉悓鍙?
    if ((cross_z[0] > 0 && cross_z[1] > 0 && cross_z[2] > 0) ||
        (cross_z[0] < 0 && cross_z[1] < 0 && cross_z[2] < 0)) {
        return true;
    }

    return false;
}

//閲嶅績鍧愭爣锛岀敤浜庢彃鍊?
std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v) {
    float c1 = (x * (v[1].y() - v[2].y()) + (v[2].x() - v[1].x()) * y + v[1].x() * v[2].y() - v[2].x() * v[1].y()) / (v[0].x() * (v[1].y() - v[2].y()) + (v[2].x() - v[1].x()) * v[0].y() + v[1].x() * v[2].y() - v[2].x() * v[1].y());
    float c2 = (x * (v[2].y() - v[0].y()) + (v[0].x() - v[2].x()) * y + v[2].x() * v[0].y() - v[0].x() * v[2].y()) / (v[1].x() * (v[2].y() - v[0].y()) + (v[0].x() - v[2].x()) * v[1].y() + v[2].x() * v[0].y() - v[0].x() * v[2].y());
    float c3 = (x * (v[0].y() - v[1].y()) + (v[1].x() - v[0].x()) * y + v[0].x() * v[1].y() - v[1].x() * v[0].y()) / (v[2].x() * (v[0].y() - v[1].y()) + (v[1].x() - v[0].x()) * v[2].y() + v[0].x() * v[1].y() - v[1].x() * v[0].y());
    return { c1,c2,c3 };
}

PixelCoverage checkPixelCoverage(int x, int y, const Vector3f* _v) {
    // 璁＄畻鍍忕礌鐨勫洓涓鐐?
    float corners[4][2] = {
        {x + 0.0f, y + 0.0f},  // 宸︿笅
        {x + 1.0f, y + 0.0f},  // 鍙充笅
        {x + 0.0f, y + 1.0f},  // 宸︿笂
        {x + 1.0f, y + 1.0f}   // 鍙充笂
    };

    int inside_count = 0;
    int total_corners = 4;

    // 妫€鏌ユ瘡涓鐐规槸鍚﹀湪涓夎褰㈠唴
    for (int i = 0; i < 4; i++) {
        if (insideTriangle(corners[i][0], corners[i][1], _v)) {
            inside_count++;
        }
    }

    if (inside_count == 0) {
        return PixelCoverage::FULLY_OUTSIDE;
    }
    else if (inside_count == total_corners) {
        return PixelCoverage::FULLY_INSIDE;
    }
    else {
        return PixelCoverage::PARTIALLY_COVERED;
    }
}

Vector3f interpolate(float alpha, float beta, float gamma, const std::array<Vector3f, 3>& source,
    const Vector3f& w, const float w_reciprocal) {
    Vector3f res = alpha * source[0] / w(0) + beta * source[1] / w(1) + gamma * source[2] / w(2);
    res *= w_reciprocal;
    return res;
}

Vector3f interpolate(float alpha, float beta, float gamma, const Vector3f* source,  // 鎴?Vector3f* source
    const Vector3f& w, const float w_reciprocal) {
    Vector3f res = alpha * source[0] / w[0] +
        beta * source[1] / w[1] +
        gamma * source[2] / w[2];
    return res * w_reciprocal;
}

Vector2f interpolate(float alpha, float beta, float gamma,
    const Vector2f* source, const Vector3f& w, const float w_reciprocal) {
    Vector2f res = alpha * source[0] / w[0] +
        beta * source[1] / w[1] +
        gamma * source[2] / w[2];
    return res * w_reciprocal;
}

Vector3f reflect(const Vector3f& vec, const Vector3f& axis) {
    auto costheta = vec.dot(axis);
    return (2 * costheta * axis - vec).normalized();
}
