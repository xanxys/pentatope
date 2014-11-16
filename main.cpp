#include <algorithm>
#include <limits>
#include <random>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <Eigen/Dense>
#include <glog/logging.h>
#include <opencv2/opencv.hpp>

#include <geometry.h>
#include <light.h>
#include <material.h>
#include <sampling.h>
#include <scene.h>
#include <space.h>


using namespace pentatope;

cv::Vec3f toCvRgb(const Spectrum& spec) {
    return cv::Vec3f(spec(2), spec(1), spec(0));
}

// A point camera that can record 2-d slice of 3-d incoming light.
// (corresponds to line camera in 3-d space)
// Points to W+ direction, and records light rays with Z=0.
class Camera2 {
public:
    Camera2(Pose pose,
            int width, int height, Radianf fov_x, Radianf fov_y) :
            pose(pose),
            width(width), height(height), fov_x(fov_x), fov_y(fov_y) {
        // TODO: check fov_x & fov_y errors.
    }

    // return 8 bit BGR image.
    cv::Mat render(
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
private:
    const Pose pose;
    const int width;
    const int height;
    const float fov_x;
    const float fov_y;
};


// TODO: serialize as prototxt
std::unique_ptr<Scene> createCornellTesseract() {
    std::unique_ptr<Scene> scene_p(new Scene(fromRgb(0, 0, 0.1)));
    Scene& scene = *scene_p;
    // Create [-1,1]^3 * [0,2] box
    // X walls: white
    // Y-:green Y+:red
    // Z-:yellow Z+:blue
    // floor(W)
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Plane(Eigen::Vector4f(0, 0, 0, 1), 0)),
        std::unique_ptr<Material>(new UniformLambertMaterial(fromRgb(1, 1, 1)))
        );
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Plane(Eigen::Vector4f(0, 0, 0, -1), -2)),
        std::unique_ptr<Material>(new UniformLambertMaterial(fromRgb(1, 1, 1)))
        );
    // walls(X)
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Plane(Eigen::Vector4f(1, 0, 0, 0), -1)),
        std::unique_ptr<Material>(new UniformLambertMaterial(fromRgb(1, 1, 1)))
        );
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Plane(Eigen::Vector4f(-1, 0, 0, 0), -1)),
        std::unique_ptr<Material>(new UniformLambertMaterial(fromRgb(1, 1, 1)))
        );
    // walls(Y)
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Plane(Eigen::Vector4f(0, 1, 0, 0), -1)),
        std::unique_ptr<Material>(new UniformLambertMaterial(fromRgb(0, 1, 0)))
        );
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Plane(Eigen::Vector4f(0, -1, 0, 0), -1)),
        std::unique_ptr<Material>(new UniformLambertMaterial(fromRgb(1, 0, 0)))
        );
    // walls(Z)
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Plane(Eigen::Vector4f(0, 0, 1, 0), -1)),
        std::unique_ptr<Material>(new UniformLambertMaterial(fromRgb(1, 1, 0)))
        );
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Plane(Eigen::Vector4f(0, 0, -1, 0), -1)),
        std::unique_ptr<Material>(new UniformLambertMaterial(fromRgb(0, 0, 1)))
        );

    // object inside room
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Sphere(Eigen::Vector4f(0, 0, 0, 0.2), 0.2)),
        std::unique_ptr<Material>(new UniformLambertMaterial(fromRgb(1, 1, 1)))
        );
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Sphere(Eigen::Vector4f(0, 0.5, 0.1, 0.5), 0.5)),
        std::unique_ptr<Material>(new GlassMaterial(1.5))
        );
    // light at center of ceiling
    /*
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Sphere(Eigen::Vector4f(0, 0, 0, 2), 0.5)),
        std::unique_ptr<Material>(new UniformEmissionMaterial(fromRgb(10, 10, 10)))
        );
    */
    scene.lights.emplace_back(
        std::unique_ptr<Light>(new PointLight(
            Eigen::Vector4f(0, 0, 0, 1.9),
            fromRgb(100, 100, 100)))
        );
    return scene_p;
}

int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    auto scene = createCornellTesseract();

    // rotation:
    // World <- Camera
    // X+         X+(horz)
    // W+         Y-(up)
    // Z+         Z+(ignored)
    // Y+         W+(forward)
    Eigen::Matrix4f cam_to_world;
    cam_to_world.col(0) = Eigen::Vector4f(1, 0, 0, 0);
    cam_to_world.col(1) = Eigen::Vector4f(0, 0, 0, -1);
    cam_to_world.col(2) = Eigen::Vector4f(0, 0, 1, 0);
    cam_to_world.col(3) = Eigen::Vector4f(0, 1, 0, 0);
    assert(cam_to_world.determinant() > 0);

    Camera2 camera(
        Pose(cam_to_world, Eigen::Vector4f(0, -0.95, 0, 1)),
        200, 200, 2.57, 2.57);

    Sampler sampler;
    cv::Mat result = camera.render(*scene, sampler, 10);
    cv::imwrite("render.png", result);

    return 0;
}
