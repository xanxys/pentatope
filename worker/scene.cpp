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
// Since we separated scattering to in-scattering and out-scattering,
// they must be balanced very accurately. Otherwise, energy conservation laws will
// be breached.
Spectrum Scene::trace(const Ray& ray, Sampler& sampler, int depth) const {
    if(depth <= 0) {
        LOG_EVERY_N(INFO, 1000000) << "trace: depth threshold reached";
        return Spectrum::Zero();
    }

    auto isect = intersect(ray);
    if(isect.first) {
        const std::unique_ptr<BSDF> o_bsdf = std::move(isect.first);
        const MicroGeometry mg = isect.second;

        Spectrum radiance_surface;
        const auto specular = o_bsdf->specular(-ray.direction);
        if(specular) {
            const auto dir = specular->first;
            // avoid self-intersection by offseting origin.
            Ray new_ray(mg.pos() + EPSILON_SURFACE_OFFSET * dir, dir);
            radiance_surface =
                specular->second.cwiseProduct(
                    trace(new_ray, sampler, depth - 1)) +
                o_bsdf->emission(-ray.direction);
        } else {
            const auto dir = sampler.uniformHemisphere(mg.normal());
            // avoid self-intersection by offseting origin.
            Ray new_ray(mg.pos() + EPSILON_SURFACE_OFFSET * dir, dir);
            radiance_surface =
                o_bsdf->bsdf(dir, -ray.direction).cwiseProduct(
                    trace(new_ray, sampler, depth - 1)) *
                (std::abs(mg.normal().dot(dir)) * pi * pi) +
                o_bsdf->emission(-ray.direction) +
                directLightToSurface(
                    mg.pos() + EPSILON_SURFACE_OFFSET * dir,
                    mg.normal(),
                    -ray.direction, *o_bsdf);
        }

        if(scattering_sigma) {
            const float dist = ray.at(isect.second.pos());

            // Attenuate by analytic solution of out-scattering.            
            Spectrum result = std::exp(-dist / *scattering_sigma) * radiance_surface;

            // Add in direct in-scattering components.
            // In this direct light calculation, no scattering will occur.
            // This is so-called single-scattering approximation.
            const int n_steps = std::ceil(dist / SCATTERING_STEP);
            for(const int i : boost::irange(0, n_steps)) {
                // Current region = [i * STEP, min((i + 1) * STEP, dist)]
                const float t0 = i * SCATTERING_STEP;
                const float t1 = std::min(dist, t0 + SCATTERING_STEP);

                // We do stratified sampling to lower variance.
                const float t_sample = std::uniform_real_distribution<float>(t0, t1)(sampler.gen);

                const float transmittance = std::exp(-t_sample / *scattering_sigma);
                result += directLightToParticle(ray.at(t_sample), -ray.direction) * transmittance * ((t1 - t0) / *scattering_sigma);
            }
            return result;
        } else {
            // Vaccum doesn't affect radiance.
            return radiance_surface;
        }
    } else {
        // Interestingly, uniform scattering do not affect radiance
        // even if it's infinitely thick.
        return background_radiance;
    }
}

// Calculate radiance that comes to pos, and reflected to dir_out.
// You must not call this for specular-only BSDFs.
Spectrum Scene::directLightToSurface(
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
        // I have a feeling that std::pow(dist, 3) cannot be separated when
        // there's a scattering.
        const float transmittance = scattering_sigma ?
            std::exp(- dist / *scattering_sigma) :
            1.0;
        const Eigen::Vector4f dir = (inten.first - pos).normalized();
        result +=
            inten.second.cwiseProduct(bsdf.bsdf(dir, dir_out)) *
            (std::abs(normal.dot(dir)) / std::pow(dist, 3)) *
            transmittance;
    }
    return result;
}

Spectrum Scene::directLightToParticle(
        const Eigen::Vector4f& pos,
        const Eigen::Vector4f& dir_out) const {
    Spectrum result = Spectrum::Zero();
    for(const auto& light : lights) {
        auto inten = light->getIntensity(pos);
        if(!isVisibleFrom(pos, inten.first)) {
            continue;
        }
        const float dist = (inten.first - pos).norm();
        const float transmittance = scattering_sigma ?
            std::exp(- dist / *scattering_sigma) :
            1.0;
        const Eigen::Vector4f dir = (inten.first - pos).normalized();
        result +=
            inten.second *
            (1 / (2 * pi * pi)) *  // uniform scattering phase function
            transmittance / std::pow(dist, 3);
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
