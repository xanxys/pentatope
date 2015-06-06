#include "scene.h"

#include <algorithm>
#include <random>

#include <boost/range/irange.hpp>
#include <glog/logging.h>

namespace pentatope {

Scene::Scene(const Spectrum& background_radiance, const boost::optional<float>& scattering_sigma) :
        background_radiance(background_radiance),
        scattering_sigma(scattering_sigma) {
}

void Scene::addObject(Object object) {
    objects.push_back(std::move(object));
}

void Scene::addLight(std::unique_ptr<Light> light) {
    lights.push_back(std::move(light));
}

void Scene::finalize() {
    accel.reset(new BVHAccel());
    accel->build(objects);
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
        Scene::intersect(const Ray& ray) const {
    assert(accel);
    return accel->intersect(ray);
}

// Samples radiance L(ray.origin, -ray.direction) by
// raytracing.
Spectrum Scene::trace(const Ray& ray, Sampler& sampler, int depth) const {
    if(depth <= 0) {
        LOG_EVERY_N(INFO, 1000000) << "trace: depth threshold reached";
        return Spectrum::Zero();
    }

    auto isect = intersect(ray);
    if(scattering_sigma) {
        // Sample particle scattering distance.
        const float t_scatter =
            std::exponential_distribution<float>(1 / *scattering_sigma)(sampler.gen);

        if(isect.first && ray.at(isect.second.pos()) < t_scatter) {
            return traceSolid(std::move(isect), ray, sampler,depth);
        } else {
            // scatter uniformly to all direction
            // TODO: direct light.
            Ray new_ray(ray.at(t_scatter), sampler.uniformSphere());
            return
                (1.0 / (2 * pi * pi)) * trace(new_ray, sampler, depth + 1);
        }
    } else {
        if(isect.first) {
            return traceSolid(std::move(isect), ray, sampler, depth);
        } else {
            return background_radiance;
        }
    }
}

Spectrum Scene::traceSolid(
        std::pair<std::unique_ptr<BSDF>, MicroGeometry>&& isect,
        const Ray& ray, Sampler& sampler, int depth) const {
    const std::unique_ptr<BSDF> o_bsdf = std::move(isect.first);
    const MicroGeometry mg = isect.second;

    const auto specular = o_bsdf->specular(-ray.direction);
    if(specular) {
        const auto dir = specular->first;
        // avoid self-intersection by offseting origin.
        Ray new_ray(mg.pos() + EPSILON_SURFACE_OFFSET * dir, dir);
        return
            specular->second.cwiseProduct(
                trace(new_ray, sampler, depth - 1)) +
            o_bsdf->emission(-ray.direction);
    } else {
        const auto dir = sampler.uniformHemisphere(mg.normal());
        // avoid self-intersection by offseting origin.
        Ray new_ray(mg.pos() + EPSILON_SURFACE_OFFSET * dir, dir);
        return
            o_bsdf->bsdf(dir, -ray.direction).cwiseProduct(
                trace(new_ray, sampler, depth - 1)) *
            (std::abs(mg.normal().dot(dir)) * pi * pi) +
            o_bsdf->emission(-ray.direction) +
            directLight(
                mg.pos() + EPSILON_SURFACE_OFFSET * dir,
                mg.normal(),
                -ray.direction, *o_bsdf);
    }
}

// Calculate radiance that comes to pos, and reflected to dir_out.
// You must not call this for specular-only BSDFs.
Spectrum Scene::directLight(
        const Eigen::Vector4f& pos,
        const Eigen::Vector4f& normal,
        const Eigen::Vector4f& dir_out, const BSDF& bsdf) const {
    Spectrum result = Spectrum::Zero();
    for(const auto& light : lights) {
        auto inten = light->getIntensity(pos);
        if(!isVisibleFrom(pos, inten.first)) {
            continue;
        }
        const float dist = (inten.first - pos).norm();
        const Eigen::Vector4f dir = (inten.first - pos).normalized();
        result +=
            inten.second.cwiseProduct(bsdf.bsdf(dir, dir_out)) *
            (std::abs(normal.dot(dir)) / std::pow(dist, 3));
    }
    return result;
}

bool Scene::isVisibleFrom(const Eigen::Vector4f& from, const Eigen::Vector4f& to) const {
    const Ray ray(from, (to - from).normalized());
    const auto isect = intersect(ray);
    if(!isect.first) {
        // no obstacle (remember, Light doesn't intersect with rays)
        return true;
    }
    return ray.at(isect.second.pos()) > (to - from).norm();
}


}  // namespace
