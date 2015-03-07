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

// Create "cornell tesseract" scene.
// TODO: serialize as prototxt under example/
// and remove this function.
std::unique_ptr<Scene> createCornellTesseract();

// fast but ugly code to get file content onto memory.
// http://stackoverflow.com/a/2602060
std::string readFile(const std::string& path);

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
// Binary proto is tried first, and then text proto.
std::tuple<std::unique_ptr<Scene>, std::unique_ptr<Camera2>, int, std::string>
    loadProtoFile(const std::string& path);

}  // namespace
