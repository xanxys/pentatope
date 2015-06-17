#include "camera.h"

#include <algorithm>
#include <thread>
#include <vector>

#include <boost/range/irange.hpp>


namespace pentatope {

cv::Vec3f toCvRgb(const Spectrum& spec) {
    return cv::Vec3f(spec(2), spec(1), spec(0));
}


Camera2::Camera2(Pose pose,
        int width, int height, Radianf fov_x, Radianf fov_y) :
        pose(pose),
        width(width), height(height), fov_x(fov_x), fov_y(fov_y) {
    if(width < 0 || height < 0) {
        throw std::runtime_error("Image size must be positive");
    }
    if(fov_x < 0 || pi <= fov_x || fov_y < 0 || pi <= fov_y) {
        throw std::runtime_error("fov_x and fov_y must be within [0, pi)");
    }
}


// return 8 bit BGR image.
cv::Mat Camera2::render(
        const Scene& scene, Sampler& sampler,
        const int samples_per_pixel,
        const int n_threads) const {
    boost::lockfree::queue<TileSpecifier> tiles(0);
    cv::Mat film(height, width, CV_32FC3);
    film = 0.0f;

    // Divide image into tiles.
    const int tile_size = 32;
    const int n_tiles_x = std::ceil(static_cast<double>(width) / tile_size);
    const int n_tiles_y = std::ceil(static_cast<double>(height) / tile_size);
    assert(n_tiles_x > 0 && n_tiles_y > 0);
    assert(n_tiles_x * tile_size >= width);
    assert(n_tiles_y * tile_size >= height);
    int n_tiles = 0;
    for(int iy : boost::irange(0, n_tiles_y)) {
        for(int ix : boost::irange(0, n_tiles_x)) {
            TileSpecifier tile;
            tile.x0 = ix * tile_size;
            tile.y0 = iy * tile_size;
            tile.dx = std::min(tile_size, width - ix * tile_size);
            tile.dy = std::min(tile_size, height - iy * tile_size);
            const bool success = tiles.push(tile);
            // insertion shouldn't & won't fail because there's only 1 thread.
            assert(success);
            n_tiles++;
        }
    }

    // Spawn workers & wait until finish.
    LOG(INFO) << "Distributing " << n_tiles << " into " << n_threads << " threads";
    assert(n_threads > 0);
    if(n_threads == 1) {
        // Don't spawn threads for easy debugging.
        workerBody(scene, sampler, samples_per_pixel, film, tiles);
    } else {
        auto child_samplers = sampler.split(n_threads);
        std::vector<std::thread> workers;
        for(int i : boost::irange(0, n_threads)) {
            workers.emplace_back(
                &Camera2::workerBody,
                this,
                std::cref(scene),
                std::ref(child_samplers[i]),
                samples_per_pixel,
                std::ref(film),
                std::ref(tiles));
        }
        for(std::thread& worker : workers) {
            worker.join();
        }
    }

    assert(tiles.empty());
    return film;
}


void Camera2::workerBody(
        const Scene& scene, Sampler& sampler,
        const int samples_per_pixel,
        cv::Mat& film,
        boost::lockfree::queue<TileSpecifier>& task_queue) const {
    while(!task_queue.empty()) {
        TileSpecifier tile;
        if(task_queue.pop(tile)) {
            renderTile(scene, sampler, samples_per_pixel, film, tile);
        } else {
            std::this_thread::yield();
        }
    }
}


void Camera2::renderTile(
        const Scene& scene, Sampler& sampler,
        const int samples_per_pixel,
        cv::Mat& film,
        TileSpecifier tile) const {
    assert(tile.dx > 0);
    assert(tile.dy > 0);
    assert(samples_per_pixel > 0);
    // TODO: use Spectrum array.
    const float c_dx = std::tan(fov_x / 2);
    const float c_dy = std::tan(fov_y / 2);
    const Eigen::Vector4f org_w = pose.asAffine().translation();
    std::uniform_real_distribution<float> px_var(-0.5, 0.5);
    for(const int y : boost::irange(tile.y0,  tile.y0 + tile.dy)) {
        for(const int x : boost::irange(tile.x0, tile.x0 + tile.dx)) {
            cv::Vec3f accum(0, 0, 0);
            for(const int i : boost::irange(0, samples_per_pixel)) {
                Eigen::Vector4f dir_c(
                    (((x + px_var(sampler.gen)) * 1.0f / width) - 0.5) * c_dx,
                    (((y + px_var(sampler.gen)) * 1.0f / height) - 0.5) * c_dy,
                    0,
                    1);
                dir_c.normalize();
                Eigen::Vector4f dir_w = pose.asAffine().rotation() * dir_c;

                Ray ray(org_w, dir_w);
                accum += toCvRgb(scene.trace(ray, sampler, 5));
            }
            film.at<cv::Vec3f>(y, x) = accum / samples_per_pixel;
        }
    }
}


cv::Mat Camera2::tonemapLinear(const cv::Mat& film) {
    const int width = film.cols;
    const int height = film.rows;

    // Get (mostly) max value.
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

    // Apply linear scaling and convert to 8-bit image.
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
