// Load external config in prototxt to renderable Scene, Camera etc.
#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include <Eigen/Dense>

#include <camera.h>
#include <scene.h>
#include <space.h>

#include <proto/render_task.pb.h>

namespace pentatope {

// This error is thrown when external task setting
// is invalid. (lacking necessary fields, contradictory settings)
class invalid_task : public std::runtime_error {
public:
    invalid_task(const std::string& what);
};

Spectrum loadSpectrum(const SpectrumProto& sp);

Eigen::Vector4f loadPoint(const Point& pt);
Eigen::Vector4f loadDirection(const Direction& pt);

std::unique_ptr<Geometry> loadGeometry(const ObjectGeometry& og);

std::unique_ptr<Material> loadMaterial(const ObjectMaterial& og);

Object loadObject(const SceneObject& so);

std::unique_ptr<Light> loadLight(const SceneLight& sl);

std::unique_ptr<Scene> loadScene(const RenderScene& rs);

std::unique_ptr<Scene> loadSceneFromRenderTask(const RenderTask& rt);

// Parse RigidTransform.
// When rotation or translation is lacking, identity will be used.
// Throws invalid_task when values (especially rotation) are invalid.
Pose loadPoseFromRigidTransform(const RigidTransform& rigid);

std::unique_ptr<Camera2> loadCameraFromCameraConfig(
    const CameraConfig& config);

// load RenderTask from given prototxt or binary proto file,
// and return (scene, camera, #samples/px, output_path)
std::tuple<std::unique_ptr<Scene>, std::unique_ptr<Camera2>, int, std::string>
    loadRenderTask(const RenderTask& task);


// fast but ugly code to get file content onto memory.
// http://stackoverflow.com/a/2602060
std::string readFile(const std::string& path);

// load RenderTask from given prototxt or binary proto file.
// Binary proto is tried first, and then text proto.
RenderTask readRenderTaskFromFile(const std::string& path);


}  // namespace
