#pragma once

#include <boost/lockfree/queue.hpp>
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

    // return 32 bit float BGR image.
    cv::Mat render(
        const Scene& scene, Sampler& sampler,
        const int samples_per_pixel,
        const int n_threads) const;
    static cv::Mat tonemapLinear(const cv::Mat& image);
private:
    // std::tuple<int, int, int, int> doesn't work because it lacks
    // boost::has_trivial_assign trait.
    struct TileSpecifier {
        int x0;
        int y0;
        int dx;
        int dy;
    };

    // Run until all tiles in task_queue is consumed.
    void workerBody(
        const Scene& scene, Sampler& sampler,
        const int sampler_per_pixel,
        cv::Mat& target,
        boost::lockfree::queue<TileSpecifier>& task_queue) const;

    // Write rendered samples to specified rectangle region of target.
    void renderTile(
        const Scene& scene, Sampler& sampler,
        const int sampler_per_pixel,
        cv::Mat& target,
        TileSpecifier tile) const;
private:
    const Pose pose;
    const int width;
    const int height;
    const float fov_x;
    const float fov_y;
};

}  // namespace
