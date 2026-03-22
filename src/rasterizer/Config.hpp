#ifndef CONFIG_H
#define CONFIG_H

//#define MSAA_OR_SSAA 0
//0表示MSAA 1表示SSAA
//如果没有define的话 就不会多重采样

#include <Eigen/Eigen>
using namespace Eigen;

constexpr double MY_PI = 3.1415926;
static const int sampleTimes = 4;
static const int totalSamplePoint = sampleTimes * sampleTimes;

enum class PixelCoverage {
    FULLY_OUTSIDE,
    FULLY_INSIDE,
    PARTIALLY_COVERED
};

struct light {
    Vector3f position;
    Vector3f intensity;
};
#endif