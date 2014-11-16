#pragma once

#include <memory>
#include <vector>

#include <Eigen/Dense>
#include <glog/logging.h>

#include <geometry.h>
#include <light.h>
#include <material.h>
#include <sampling.h>
#include <space.h>

namespace pentatope {

// Complete collection of visually relevant things.
// Provides radiance interface (trace) externally.
class Scene {
public:
    Scene(const Spectrum& background_radiance);

    // std::unique_ptr is not nullptr if valid, otherwise invalid
    // (MicroGeometry will be undefined).
    //
    // TODO: use
    // std::optional<std::pair<std::unique_ptr<BSDF>, MicroGeometry>>
    //
    // upgrading boost is too cumbersome; wait for C++14.
    // boost::optional 1.53.0 doesn't support move semantics,
    // so optional<...unique_ptr is impossible to use although desirable.
    // that's why I'm stuck with this interface.
    std::pair<std::unique_ptr<BSDF>, MicroGeometry>
            intersect(const Ray& ray) const;

    // Samples radiance L(ray.origin, -ray.direction) by
    // raytracing.
    Spectrum trace(const Ray& ray, Sampler& sampler, int depth) const;


    // Calculate radiance that comes to pos, and reflected to dir_out.
    // You must not call this for specular-only BSDFs.
    Spectrum directLight(
        const Eigen::Vector4f& pos,
        const Eigen::Vector4f& normal,
        const Eigen::Vector4f& dir_out, const BSDF& bsdf) const;

    bool isVisibleFrom(
		const Eigen::Vector4f& from, const Eigen::Vector4f& to) const;
public:
    std::vector<std::pair<
        std::unique_ptr<Geometry>,
        std::unique_ptr<Material>>> objects;
    std::vector<std::unique_ptr<Light>> lights;
    Spectrum background_radiance;
};

}  // namespace
