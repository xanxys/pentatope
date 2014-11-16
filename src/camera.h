#pragma once

#include <opencv2/opencv.hpp>

#include <sampling.h>
#include <scene.h>
#include <space.h>

namespace pentatope {

cv::Vec3f toCvRgb(const Spectrum& spec);

// A point camera that can record 2-d slice of 3-d incoming light.
// (corresponds to line camera in 3-d space)
// Points to W+ direction, and records light rays with Z=0.
class Camera2 {
public:
    Camera2(Pose pose,
            int width, int height, Radianf fov_x, Radianf fov_y);

    // return 8 bit BGR image.
    cv::Mat render(
        const Scene& scene, Sampler& sampler,
        const int samples_per_pixel) const;

private:
    const Pose pose;
    const int width;
    const int height;
    const float fov_x;
    const float fov_y;
};

}  // namespace
