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
#include <space.h>


using namespace pentatope;

cv::Vec3f toCvRgb(const Spectrum& spec) {
    return cv::Vec3f(spec(2), spec(1), spec(0));
}

class Sampler {
public:
    Sampler() {
    }

    Eigen::Vector4f uniformHemisphere(const Eigen::Vector4f& normal) {
        std::uniform_real_distribution<float> interval(-1, 1);
        Eigen::Vector4f result;
        while(true) {
            result = Eigen::Vector4f(
                interval(gen),
                interval(gen),
                interval(gen),
                interval(gen));
            const float radius = result.norm();
            if(radius > 1 || radius == 0) {
                continue;
            }
            const float cosine = result.dot(normal);
            if(cosine >= 0) {
                result /= radius;
            } else {
                result /= -radius;
            }
            return result;
        }
    }
public:
    std::mt19937 gen;
};



// Complete collection of visually relevant things.
class Scene {
public:
    Scene() {
        background_radiance = fromRgb(0, 0, 0.1);
    }

    // std::unique_ptr is not nullptr if valid, otherwise invalid
    // (MicroGeometry will be undefined).
    //
    // TODO: use std::optional<std::pair<std::unique_ptr<BSDF>, MicroGeometry>>
    //
    // upgrading boost is too cumbersome; wait for C++14.
    // boost::optional 1.53.0 doesn't support move semantics,
    // so optional<...unique_ptr is impossible to use although desirable.
    // that's why I'm stuck with this interface.
    std::pair<std::unique_ptr<BSDF>, MicroGeometry>
            intersect(const Ray& ray) const {
        float t_min = std::numeric_limits<float>::max();
        std::pair<std::unique_ptr<BSDF>, MicroGeometry> isect_nearest;

        for(const auto& object : objects) {
            auto isect = object.first->intersect(ray);
            if(!isect) {
                continue;
            }
            const float t = ray.at(isect->pos());
            if(t < t_min) {
                isect_nearest.first.reset(
                    object.second->getBSDF(*isect).release());
                isect_nearest.second = *isect;
                t_min = t;
            }
        }
        return isect_nearest;
    }

    // Samples radiance L(ray.origin, -ray.direction) by
    // raytracing.
    Spectrum trace(const Ray& ray, Sampler& sampler, int depth) const {
        if(depth <= 0) {
            LOG_EVERY_N(INFO, 1000000) << "trace: depth threshold reached";
            return Spectrum::Zero();
        }

        auto isect = intersect(ray);
        if(isect.first) {
            const std::unique_ptr<BSDF> o_bsdf = std::move(isect.first);
            const MicroGeometry mg = isect.second;
            const float epsilon = 1e-6;
            const auto dir = sampler.uniformHemisphere(mg.normal());
            // avoid self-intersection by offseting origin.
            Ray new_ray(mg.pos() + epsilon * dir, dir);
            return
                o_bsdf->bsdf(dir, -ray.direction).cwiseProduct(
                    trace(new_ray, sampler, depth - 1)) *
                (mg.normal().dot(dir) * pi * pi) +
                o_bsdf->emission(-ray.direction);
        } else {
            return background_radiance;
        }
    }

public:
    std::vector<std::pair<std::unique_ptr<Geometry>, std::unique_ptr<Material>>> objects;
    Spectrum background_radiance;
};




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
        float max_v = 0;
        for(int y = 0; y < height; y++) {
            for(int x = 0; x < width; x++) {
                const auto v = film.at<cv::Vec3f>(y, x);
                max_v = std::max({max_v, v[0], v[1], v[2]});
            }
        }
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
    std::unique_ptr<Scene> scene_p(new Scene());
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
    // light at center of ceiling
    scene.objects.emplace_back(
        std::unique_ptr<Geometry>(new Sphere(Eigen::Vector4f(0, 0, 0, 2), 0.5)),
        std::unique_ptr<Material>(new UniformEmissionMaterial(fromRgb(10, 10, 10)))
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
    cv::Mat result = camera.render(*scene, sampler, 100);
    cv::imwrite("render.png", result);

    return 0;
}
