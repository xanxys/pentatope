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

Spectrum loadSpectrum(const SpectrumProto& sp) {
    if(!sp.has_r() || !sp.has_g() || !sp.has_b()) {
        throw invalid_task("Spectrum requires r, g, b");
    }
    return fromRgb(sp.r(), sp.g(), sp.b());
}

Eigen::Vector4f loadPoint(const Point& pt) {
    return Eigen::Vector4f(pt.x(), pt.y(), pt.z(), pt.w());
}

Eigen::Vector4f loadDirection(const Direction& dir) {
    const Eigen::Vector4f d(dir.x(), dir.y(), dir.z(), dir.w());
    if(std::abs(d.norm() - 1) >= 1e-5) {
        throw invalid_task("Length of direction must be 1.");
    }
    return d;
}

std::unique_ptr<Geometry> loadGeometry(const ObjectGeometry& og) {
    if(og.type() == ObjectGeometry::OBB) {
        const OBBGeometry& obb =
            og.GetExtension(OBBGeometry::geom);
        if(!obb.has_local_to_world()) {
            throw invalid_task("OBB requires local_to_world");
        }
        if(obb.size_size() != 4) {
            throw invalid_task("size must be 4-dimensional");
        }
        Eigen::Vector4f size;
        const auto size_elements = obb.size();
        for(const int ix : boost::irange(0, 4)) {
            size(ix) = size_elements.Get(ix);
            if(size(ix) <= 0) {
                throw invalid_task("Size must be positive");
            }
        }
        return std::make_unique<OBB>(
            loadPoseFromRigidTransform(obb.local_to_world()),
            size);
    } else if(og.type() == ObjectGeometry::TETRAHEDRON) {
        const TetrahedronGeometry& tetra =
            og.GetExtension(TetrahedronGeometry::geom);
        return std::make_unique<Tetrahedron>(
            std::array<Eigen::Vector4f, 4>({
                loadPoint(tetra.vertex0()),
                loadPoint(tetra.vertex1()),
                loadPoint(tetra.vertex2()),
                loadPoint(tetra.vertex3())
                }));
    } else if(og.type() == ObjectGeometry::SPHERE) {
        const SphereGeometry& sphere =
            og.GetExtension(SphereGeometry::geom);
        if(sphere.radius() <= 0) {
            throw invalid_task("Sphere radius must be positive");
        }
        return std::make_unique<Sphere>(
            loadPoint(sphere.center()),
            sphere.radius());
    } else if(og.type() == ObjectGeometry::DISC) {
        const DiscGeometry& disc =
            og.GetExtension(DiscGeometry::geom);
        if(disc.radius() <= 0) {
            throw invalid_task("Disc radius must be positive");
        }
        return std::make_unique<Disc>(
            loadPoint(disc.center()),
            loadDirection(disc.normal()),
            disc.radius());
    } else {
        throw invalid_task("Unknown geometry type");
    }
}

std::unique_ptr<Material> loadMaterial(const ObjectMaterial& om) {
    if(om.type() == ObjectMaterial::UNIFORM_LAMBERT) {
        const UniformLambertMaterialProto& material_proto =
            om.GetExtension(
                UniformLambertMaterialProto::material);
        if(!material_proto.has_reflectance()) {
            throw invalid_task("UniformLambertMaterial requires reflectance.");
        }
        const Spectrum reflectance = loadSpectrum(material_proto.reflectance());

        if(!reflectance.allFinite() ||
                reflectance.minCoeff() < 0 ||
                reflectance.maxCoeff() > 1) {
            throw invalid_task("Reflectance must be within 0 and 1.");
        }
        return std::make_unique<UniformLambertMaterial>(reflectance);
    } else if(om.type() == ObjectMaterial::GLASS) {
        const GlassMaterialProto& glass_proto =
            om.GetExtension(GlassMaterialProto::material);
        const float refractive_index = glass_proto.has_refractive_index() ?
            glass_proto.refractive_index() : 1.0;
        return std::make_unique<GlassMaterial>(refractive_index);
    } else {
        throw invalid_task("Unknown material type");
    }
}

Object loadObject(const SceneObject& object) {
    // Load geometry.
    if(!object.has_geometry()) {
        throw invalid_task("Object requires geometry.");
    }
    std::unique_ptr<Geometry> geom = loadGeometry(object.geometry());
    // Load material.
    if(!object.has_material()) {
        throw invalid_task("Object requires material.");
    }
    std::unique_ptr<Material> material = loadMaterial(object.material());
    // Construct and append object.
    assert(geom);
    assert(material);
    return std::make_pair(std::move(geom), std::move(material));
}

std::unique_ptr<Light> loadLight(const SceneLight& light_proto) {
    std::unique_ptr<Light> light;
    if(light_proto.type() == SceneLight::POINT) {
        const PointLightProto& point_light =
            light_proto.GetExtension(PointLightProto::light);
        if(point_light.translation_size() != 4) {
            throw invalid_task("PointLight translation must be 4-dimensional");
        }
        if(!point_light.has_power()) {
            throw invalid_task("PointLight requires power specification");
        }
        Eigen::Vector4f trans;
        for(const int ix : boost::irange(0, 4)) {
            trans(ix) = point_light.translation().Get(ix);
        }
        return std::make_unique<PointLight>(
            trans, loadSpectrum(point_light.power()));
    } else {
        throw invalid_task("Unknown light type");
    }
}

std::unique_ptr<Scene> loadScene(const RenderScene& rs) {
    Spectrum background(fromRgb(0, 0, 0));
    if(rs.has_background_radiance()) {
        background = loadSpectrum(rs.background_radiance());
    }
    boost::optional<float> scattering_sigma;
    if(rs.has_uniform_scattering()) {
        if(rs.uniform_scattering().sigma() <= 0) {
            throw invalid_task("Scattering sigma must be positive");
        }
        scattering_sigma = rs.uniform_scattering().sigma();
    }
    std::unique_ptr<Scene> scene_p(new Scene(background, scattering_sigma));
    Scene& scene = *scene_p;
    for(const auto& object : rs.objects()) {
        scene.addObject(loadObject(object));
    }
    for(const auto& light_proto : rs.lights()) {
        scene.addLight(loadLight(light_proto));
    }
    return scene_p;
}

std::unique_ptr<Scene> loadSceneFromRenderTask(const RenderTask& rt) {
    std::unique_ptr<Scene> scene;
    if(rt.has_scene()) {
        scene = loadScene(rt.scene());
    } else {
        throw invalid_task(
            "Scene specification not found");
    }
    assert(scene);
    scene->finalize();
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
        std::string rot_str;
        google::protobuf::TextFormat::PrintToString(rigid, &rot_str);
        throw std::runtime_error(
            "invalid rotation (determinant must be 1)\n" + rot_str);
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

// parse RenderTask from given prototxt file,
// and return (scene, camera, #samples/px)
std::tuple<std::unique_ptr<Scene>, std::unique_ptr<Camera2>, int>
        loadRenderTask(const RenderTask& rt) {
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

    return std::make_tuple(
        std::move(scene), std::move(camera), sample_per_px);
}

RenderTask readRenderTaskFromFile(const std::string& path) {
    // Load to on-memory string since google:: streams are hard to use.
    const std::string proto = readFile(path);
    RenderTask rt;
    if(!rt.ParseFromString(proto)) {
        if(!google::protobuf::TextFormat::ParseFromString(proto, &rt)) {
            throw std::runtime_error(
                "Couldn't parse RenderTask as either prototxt or binary proto");
        }
    }
    return rt;
}



}  // namespace
