#include "camera.h"

#include <algorithm>
#include <vector>

namespace pentatope {

cv::Vec3f toCvRgb(const Spectrum& spec) {
    return cv::Vec3f(spec(2), spec(1), spec(0));
}


Camera2::Camera2(Pose pose,
        int width, int height, Radianf fov_x, Radianf fov_y) :
        pose(pose),
        width(width), height(height), fov_x(fov_x), fov_y(fov_y) {
    // TODO: check fov_x & fov_y errors.
}

// return 8 bit BGR image.
cv::Mat Camera2::render(
        const Scene& scene, Sampler& sampler,
        const int samples_per_pixel) const {
    // TODO: use Spectrum array.
    cv::Mat film(height, width, CV_32FC3);
    film = 0.0f;
    const float dx = std::tan(fov_x / 2);
    const float dy = std::tan(fov_y / 2);
    const Eigen::Vector4f org_w = pose.asAffine().translation();
    std::uniform_real_distribution<float> px_var(-0.5, 0.5);
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            Eigen::Vector4f dir_c(
                (((x + px_var(sampler.gen)) * 1.0f / width) - 0.5) * dx,
                (((y + px_var(sampler.gen)) * 1.0f / height) - 0.5) * dy,
                0,
                1);
            dir_c.normalize();
            Eigen::Vector4f dir_w = pose.asAffine().rotation() * dir_c;

            Ray ray(org_w, dir_w);
            for(int i = 0; i < samples_per_pixel; i++) {
                film.at<cv::Vec3f>(y, x) += toCvRgb(scene.trace(ray, sampler, 5));
            }
        }
    }
    film /= samples_per_pixel;

    // tonemap.
    const float disp_gamma = 2.2;
    std::vector<float> vs;
    vs.reserve(height * width);
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            const auto v = film.at<cv::Vec3f>(y, x);
            vs.push_back(std::max({v[0], v[1], v[2]}));
        }
    }
    std::sort(vs.begin(), vs.end());
    const float max_v = vs[static_cast<int>(vs.size() * 0.99)];
    LOG(INFO) << "Linear tonemapper: min=" << vs[0] << " 99%=" << max_v;
    cv::Mat image(height, width, CV_8UC3);
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            cv::Vec3f color = film.at<cv::Vec3f>(y, x) / max_v;
            for(int channel = 0; channel < 3; channel++) {
                color[channel] = std::pow(color[channel], 1 / disp_gamma);
            }
            color *= 255;
            image.at<cv::Vec3b>(y, x) = color;
        }
    }
    return image;
}


}  // namespace
