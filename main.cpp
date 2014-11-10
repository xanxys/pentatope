#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <glog/logging.h>
#include <opencv2/opencv.hpp>

const float pi = 3.14159265359;

// This error is thrown when someone tries to do physically
// impossible things.
class physics_error : public std::logic_error {
public:
    physics_error(const std::string& what) : std::logic_error(what) {
    }
};


// Use this to represent (relative) angle to avoid
// confusion over radian vs degree.
using Radianf = float;

// pose in 4-d space. (4 translational DoF + 6 rotational DoF)
// represented by local to parent transform.
class Pose {
public:
    // identity.
    Pose() : pose(Eigen::Transform<float, 4, Eigen::Affine>::Identity()) {
    }

    Eigen::Transform<float, 4, Eigen::Affine> asAffine() const {
        return pose;
    }
private:
    Eigen::Transform<float, 4, Eigen::Affine> pose;
};

// intersection range: (0, +inf)
class Ray {
public:
    Ray(Eigen::Vector4f origin, Eigen::Vector4f direction) :
        origin(origin), direction(direction) {
    }

    Eigen::Vector4f at(float t) const {
        return origin + direction * t;
    }
public:
    const Eigen::Vector4f origin;
    const Eigen::Vector4f direction;
};



class Sphere {
public:
    Sphere(Eigen::Vector4f center, float radius) :
            center(center), radius(radius) {
    }

    bool intersect(const Ray& ray,
            float* t, Eigen::Vector4f* pos, Eigen::Vector4f* normal) const {
        const Eigen::Vector4f delta = ray.origin - center;
        // turn into a quadratic equation at^2+bt+c=0
        const float a = std::pow(ray.direction.norm(), 2);
        const float b = 2 * delta.dot(ray.direction);
        const float c = std::pow(delta.norm(), 2) - std::pow(radius, 2);
        const float det = b * b - 4 * a * c;
        if(det < 0) {
            return false;
        }
        const float t0 = (-b - std::sqrt(det)) / (2 * a);
        const float t1 = (-b + std::sqrt(det)) / (2 * a);
        float t_isect;
        if(t0 > 0) {
            t_isect = t0;
        } else if(t1 > 0) {
            t_isect = t1;
        } else {
            return false;
        }
        // store intersection
        const Eigen::Vector4f p = ray.at(t_isect);
        if(t) {
            *t = t_isect;
        }
        if(pos) {
            *pos = p;
        }
        if(normal) {
            *normal = (p - center).normalized();
        }
        return true;
    }
private:
    const Eigen::Vector4f center;
    const float radius;
};


class Scene {
public:
    bool intersect(const Ray& ray,
            bool* o_type, Eigen::Vector4f* o_pos, Eigen::Vector4f* o_normal) const {
        float t_min = std::numeric_limits<float>::max();
        bool isect = false;

        for(const auto& object : objects) {
            float t;
            Eigen::Vector4f pos;
            Eigen::Vector4f normal;
            if(object.first.intersect(ray, &t, &pos, &normal)) {
                if(t < t_min) {
                    isect = true;
                    t_min = t;
                    if(o_type) {
                        *o_type = object.second;
                    }
                    if(o_pos) {
                        *o_pos = pos;
                    }
                    if(o_normal) {
                        *o_normal = normal;
                    }
                }
            }
        }
        return isect;
    }
    std::vector<std::pair<Sphere, bool>> objects;
};


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
    cv::Mat render(const Scene& scene, Sampler& sampler) const {
        const int samples_per_pixel = 1000;
        cv::Mat film(height, width, CV_32FC3);
        film = 0.0f;
        const float dx = std::tan(fov_x / 2);
        const float dy = std::tan(fov_y / 2);
        const Eigen::Vector4f org = pose.asAffine().translation();
        for(int y = 0; y < height; y++) {
            for(int x = 0; x < width; x++) {
                Eigen::Vector4f dir(
                    ((x * 1.0f / width) - 0.5) * dx,
                    ((y * 1.0f / height) - 0.5) * dy,
                    0,
                    1);
                dir.normalize();

                Ray ray(org, dir);
                for(int i = 0; i < samples_per_pixel; i++) {
                    film.at<cv::Vec3f>(y, x) += trace(ray, scene, sampler);
                }
            }
        }
        film /= samples_per_pixel;

        // tonemap.
        cv::Mat image(height, width, CV_8UC3);
        for(int y = 0; y < height; y++) {
            for(int x = 0; x < width; x++) {
                const cv::Vec3b color = film.at<cv::Vec3f>(y, x) * 255;
                image.at<cv::Vec3b>(y, x) = color;
            }
        }
        return image;
    }

    cv::Vec3f trace(const Ray& ray, const Scene& scene, Sampler& sampler) const {
        const cv::Vec3f light_radiance(100, 100, 100);

        bool o_type;
        Eigen::Vector4f pos;
        Eigen::Vector4f normal;
        if(scene.intersect(ray, &o_type, &pos, &normal)) {
            if(o_type) {
                // light
                return light_radiance;
            } else {
                // lambert
                const float epsilon = 1e-6;
                const float brdf = 0.1;  // TODO: calculate this
                const auto dir = sampler.uniformHemisphere(normal);
                // avoid self-intersection by offseting origin.
                Ray new_ray(pos + epsilon * dir, dir);
                
                return brdf * normal.dot(dir) * pi * pi * trace(new_ray, scene, sampler);
            }
        } else {
            return cv::Vec3f(0.3, 0, 0);
        }
    }
private:
    const int width;
    const int height;
    const float fov_x;
    const float fov_y;
    const Pose pose;
};


int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    Scene scene;
    scene.objects.emplace_back(
        Sphere(Eigen::Vector4f(0, 0, 0, 5), 1),
        false);  // object
    scene.objects.emplace_back(
        Sphere(Eigen::Vector4f(3, 0, 0, 0), 0.5),
        true);  // light

    Camera2 camera(Pose(), 100, 100, 1.0, 1.0);

    Sampler sampler;
    cv::Mat result = camera.render(scene, sampler);
    cv::imwrite("render.png", result);

    return 0;
}
