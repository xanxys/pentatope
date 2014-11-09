#include <stdexcept>
#include <string>

#include <Eigen/Dense>
#include <glog/logging.h>
#include <opencv2/opencv.hpp>

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
public:
    const Eigen::Vector4f origin;
    const Eigen::Vector4f direction;
};



class Sphere {
public:
    Sphere(Eigen::Vector4f center, float radius) :
            center(center), radius(radius) {
    }

    bool intersect(const Ray& ray, float* t) const {
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
        if(t0 > 0) {
            if(t) {
                *t = t0;
            }
            return true;
        } else if(t1 > 0) {
            if(t) {
                *t = t1;
            }
            return true;
        } else {
            return false;
        }
    }
private:
    const Eigen::Vector4f center;
    const float radius;
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
    cv::Mat render() const {
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
                film.at<cv::Vec3f>(y, x) = trace(ray);
            }
        }

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

    cv::Vec3f trace(const Ray& ray) const {
        Sphere sphere(Eigen::Vector4f(0, 0, 0, 5), 1);

        if(sphere.intersect(ray, nullptr)) {
            return cv::Vec3f(ray.direction(0), 0, 0);
        } else {
            return cv::Vec3f(0, 0, 0);
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

    Camera2 camera(Pose(), 100, 100, 1.0, 1.0);
    cv::Mat result = camera.render();
    cv::imwrite("render.png", result);

    return 0;
}
