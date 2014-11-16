#include <fstream>
#include <memory>
#include <stdexcept>
#include <streambuf>
#include <string>

#include <Eigen/Dense>
#include <glog/logging.h>
#include <google/protobuf/text_format.h>
#include <opencv2/opencv.hpp>

#include <camera.h>
#include <geometry.h>
#include <light.h>
#include <material.h>
#include <sampling.h>
#include <scene.h>
#include <space.h>

#include <proto/render_task.pb.h>

using namespace pentatope;

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

// fast but ugly code to get file content onto memory.
// http://stackoverflow.com/a/2602060
std::string readFile(const std::string& path) {
    std::ifstream t(path);
    if(t.fail()) {
        throw std::runtime_error("Failed to open " + path);
    }
    std::string str;

    t.seekg(0, std::ios::end);
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign(
        (std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());
    return str;
}

std::unique_ptr<Scene> createSceneFromTextProtoFile(const std::string& path) {
    // Load to on-memory string since google:: streams are hard to use.
    std::string proto = readFile(path);

    RenderTask rt;
    if(!google::protobuf::TextFormat::ParseFromString(proto, &rt)) {
        throw std::runtime_error("Couldn't parse RenderTask prototxt");
    }

    LOG(INFO) << "RenderTask loaded: " <<
        (rt.has_scene_name() ? rt.scene_name() : "<scene name unspecified>");

    return std::unique_ptr<Scene>();
}

int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    auto scene = createCornellTesseract();

    createSceneFromTextProtoFile("example/cornell_tesseract.prototxt");

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
