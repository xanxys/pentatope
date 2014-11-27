#include "loader.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <streambuf>
#include <string>

#include <boost/range/irange.hpp>
#include <Eigen/Dense>
#include <glog/logging.h>
#include <google/protobuf/text_format.h>

#include <camera.h>
#include <geometry.h>
#include <light.h>
#include <material.h>
#include <sampling.h>
#include <scene.h>
#include <space.h>

namespace pentatope {

invalid_task::invalid_task(const std::string& what) :
		std::runtime_error(what) {
}


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

std::unique_ptr<Scene> loadSceneFromRenderTask(const RenderTask& rt) {
    std::unique_ptr<Scene> scene;
    if(rt.has_scene_name()) {
        if(rt.scene_name() == "cornell") {
            scene = createCornellTesseract();
        } else {
            throw invalid_task(
                "Unknown scene name: " + rt.scene_name());
        }
    } else {
        throw invalid_task(
            "Scene specification in prototxt is not supported yet");
    }
    assert(scene);
    return scene;
}

// Parse RigidTransform.
// When rotation or translation is lacking, identity will be used.
// Throws invalid_task when values (especially rotation) are invalid.
Pose loadPoseFromRigidTransform(const RigidTransform& rigid) {
    Eigen::Matrix4f rot = Eigen::Matrix4f::Identity();
    Eigen::Vector4f trans = Eigen::Vector4f::Zero();
    // rot
    if(rigid.rotation_size() != 0) {
        if(rigid.rotation_size() != 16) {
            throw invalid_task("Rotation matrix must be 4x4");
        }
        const auto rot_elements = rigid.rotation();
        for(const int ix : boost::irange(0, 16)) {
            const int row = ix / 4;
            const int col = ix % 4;
            rot(row, col) = rot_elements.Get(ix);
        }
    } else {
        LOG(WARNING) << "rotation not found; defaults to no rotation";
    }
    if(std::abs(rot.determinant() - 1) > 1e-6) {
        throw std::runtime_error("invalid rotation (determinant must be 1)");
    }
    // trans
    if(rigid.translation_size() != 0) {
        if(rigid.translation_size() != 4) {
            throw invalid_task("translation must be 4-dimensional");
        }
        const auto trans_elements = rigid.translation();
        for(const int ix : boost::irange(0, 4)) {
            trans(ix) = trans_elements.Get(ix);
        }
    } else {
        LOG(WARNING) << "translation not found; defaults to origin";
    }
    LOG(INFO) << "pose rotation: " << rot << " / trans: " << trans;
    return Pose(rot, trans);
}

std::unique_ptr<Camera2> loadCameraFromCameraConfig(const CameraConfig& config) {
    std::unique_ptr<Camera2> camera;
    if(!config.has_camera_type()) {
        throw invalid_task("camera_type not found");
    }
    if(config.camera_type() == "perspective2") {
        Pose pose(Eigen::Matrix4f::Identity(), Eigen::Vector4f::Zero());
        if(!config.has_local_to_world()) {
            LOG(WARNING) << "local_to_world not found; defaults to identity transform";
        } else {
            pose = loadPoseFromRigidTransform(config.local_to_world());
        }
        if(!config.has_size_x() || !config.has_size_y()) {
            throw invalid_task("camera image size not found");
        }
        if(!config.has_fov_x() || !config.has_fov_y()) {
            throw invalid_task("camera fov not found");
        }
        if(config.fov_x() <= 0 || config.fov_x() >= 180) {
            throw invalid_task("fov_x must be in (0, 180)");
        }
        if(config.fov_y() <= 0 || config.fov_y() >= 180) {
            throw invalid_task("fov_y must be in (0, 180)");
        }
        camera.reset(new Camera2(
            pose,
            config.size_x(),
            config.size_y(),
            config.fov_x() * (pi / 180),
            config.fov_y() * (pi / 180)));
    } else {
        throw invalid_task(
            "Unknown camera_type: " + config.camera_type());
    }
    return camera;
}

// load RenderTask from given prototxt file,
// and return (scene, camera, #samples/px)
std::tuple<std::unique_ptr<Scene>, std::unique_ptr<Camera2>, int, std::string>
        loadProtoFile(const std::string& path) {
    // Load to on-memory string since google:: streams are hard to use.
    const std::string proto = readFile(path);
    RenderTask rt;
    if(!rt.ParseFromString(proto)) {
        if(!google::protobuf::TextFormat::ParseFromString(proto, &rt)) {
            throw std::runtime_error(
                "Couldn't parse RenderTask as either prototxt or binary proto");
        }
    }
    LOG(INFO) << "RenderTask loaded: " <<
        (rt.has_scene_name() ? rt.scene_name() : "<scene name unspecified>");

    std::unique_ptr<Scene> scene = loadSceneFromRenderTask(rt);

    if(!rt.has_camera()) {
        throw std::runtime_error("camera not found");
    }
    std::unique_ptr<Camera2> camera = loadCameraFromCameraConfig(rt.camera());

    // Check sampling settings.
    if(!rt.has_sample_per_pixel()) {
        throw invalid_task("sample_per_px not found");
    }
    int sample_per_px = rt.sample_per_pixel();
    if(sample_per_px <= 0) {
        throw physics_error("sampler_per_px must be > 0");
    }

    // Check output settings.
    if(!rt.has_output_path()) {
        throw invalid_task("output_path not found");
    }

    return std::make_tuple(
        std::move(scene), std::move(camera), sample_per_px, rt.output_path());
}

}  // namespace
