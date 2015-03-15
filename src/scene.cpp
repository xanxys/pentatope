#include "scene.h"

#include <algorithm>
#include <random>

#include <boost/range/irange.hpp>
#include <glog/logging.h>

namespace pentatope {

void BruteForceAccel::build(
        const std::vector<Object>& objects) {
    for(const auto& object : objects) {
        object_refs.push_back(object);
    }
}

std::pair<std::unique_ptr<BSDF>, MicroGeometry>
        BruteForceAccel::intersect(const Ray& ray) const {
    float t_min = std::numeric_limits<float>::max();
    std::pair<std::unique_ptr<BSDF>, MicroGeometry> isect_nearest;

    for(const auto object : object_refs) {
        const bool aabb_isect = object.get().first->bounds().intersect(ray);
        auto isect = object.get().first->intersect(ray);
        if(isect && !aabb_isect) {
            LOG(WARNING) << "Discprenacy found" <<
                "Ray: " << ray.origin << " : " << ray.direction << " / "
                "AABB: " <<
                    object.get().first->bounds().min() << " : " <<
                    object.get().first->bounds().max() << " / ";

            LOG(WARNING) << "Plane normal=" <<
                dynamic_cast<Plane*>(object.get().first.get())->normal << " / " <<
                "d=" << dynamic_cast<Plane*>(object.get().first.get())->d;

        }
        if(!isect) {
            continue;
        }
        const float t = ray.at(isect->pos());
        if(t < t_min) {
            isect_nearest.first.reset(
                object.get().second->getBSDF(*isect).release());
            isect_nearest.second = *isect;
            t_min = t;
        }
    }
    return isect_nearest;
}


void BVHAccel::build(const std::vector<Object>& objects) {
    if(objects.empty()) {
        root.release();
    } else {
        std::vector<std::reference_wrapper<const Object>>
            object_refs;
        for(const auto& object : objects) {
            object_refs.push_back(object);
        }
        root = buildTree(object_refs);
    }
}

std::unique_ptr<BVHAccel::BVHNode> BVHAccel::buildTree(
        const std::vector<
            std::reference_wrapper<const Object>>& objects) const {
    const int minimum_objects_per_node = 3;
    assert(!objects.empty());
    // Calculate all AABBs and their union.
    std::vector<AABB> aabbs;
    for(const auto& object : objects) {
        aabbs.push_back(object.get().first->bounds());
    }
    const auto aabb_whole = AABB::fromAABBs(aabbs);
    auto node = std::make_unique<BVHNode>();
    node->aabb = aabb_whole;
    // Create a leaf when object is few.
    if(objects.size() <= minimum_objects_per_node) {
        for(const auto obj_ref : objects) {
            node->objects.push_back(obj_ref);
        }
        return node;
    }
    // Otherwise, create a branch.
    float longest_size = 0;
    int longest_axis = -1;
    for(const int axis : boost::irange(0, 4)) {
        const float size = aabb_whole.size()(axis);
        if(size > longest_size) {
            longest_size = size;
            longest_axis = axis;
        }
    }
    assert(longest_axis >= 0);
    const float midpoint = aabb_whole.center()(longest_axis);
    // Partition objects by comparing centroids of objects
    // with midpoint of the chosen axis.
    std::vector<
        std::reference_wrapper<const Object>> children0;
    std::vector<
        std::reference_wrapper<const Object>> children1;
    for(const auto obj_ref : objects) {
        if(obj_ref.get().first->bounds().center()(longest_axis)
                < midpoint) {
            children0.push_back(obj_ref);
        } else {
            children1.push_back(obj_ref);
        }
    }
    assert(children0.size() + children1.size() == objects.size());
    if(!children0.empty() && !children1.empty()) {
        node->left = buildTree(children0);
        node->right = buildTree(children1);
        return node;
    }
    LOG(WARNING) << "Biased BVH tree; expect poor performance n=" << objects.size();
    // Create a leaf in a pathological case.
    // TODO: better handling
    for(const auto obj_ref : objects) {
        node->objects.push_back(obj_ref);
    }
    return node;
}

std::pair<std::unique_ptr<BSDF>, MicroGeometry>
        BVHAccel::intersect(const Ray& ray) const {
    if(!root) {
        return std::make_pair(nullptr, MicroGeometry());
    }
    return intersectTree(*root, ray);
}

std::pair<std::unique_ptr<BSDF>, MicroGeometry>
        BVHAccel::intersectTree(const BVHNode& node, const Ray& ray) const {
    if(!node.aabb.intersect(ray)) {
        return std::make_pair(nullptr, MicroGeometry());
    }
    if(!node.objects.empty()) {
        // leaf
        assert(!node.left && !node.right);
        float t_min = std::numeric_limits<float>::max();
        std::pair<std::unique_ptr<BSDF>, MicroGeometry> isect_nearest;
        for(const auto object : node.objects) {
            auto isect = object.get().first->intersect(ray);
            if(!isect) {
                continue;
            }
            const float t = ray.at(isect->pos());
            if(t < t_min) {
                isect_nearest.first.reset(
                    object.get().second->getBSDF(*isect).release());
                isect_nearest.second = *isect;
                t_min = t;
            }
        }
        return isect_nearest;
    } else {
        // branch
        assert(node.left && node.right);
        auto isect_left = intersectTree(*node.left, ray);
        auto isect_right = intersectTree(*node.right, ray);
        if(isect_left.first && isect_right.first) {
            const float t_left = ray.at(isect_left.second.pos());
            const float t_right = ray.at(isect_right.second.pos());
            if(t_left < t_right) {
                return isect_left;
            } else {
                return isect_right;
            }
        } else if(isect_left.first) {
            return isect_left;
        } else if(isect_right.first) {
            return isect_right;
        } else {
            return std::make_pair(nullptr, MicroGeometry());
        }
    }
}

BVHAccel::BVHNode::BVHNode() :
    aabb(Eigen::Vector4f::Zero(), Eigen::Vector4f::Zero()) {
}


Scene::Scene(const Spectrum& background_radiance) :
        background_radiance(background_radiance) {
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
    if(isect.first) {
        const std::unique_ptr<BSDF> o_bsdf = std::move(isect.first);
        const MicroGeometry mg = isect.second;
        const float epsilon = 1e-6;

        const auto specular = o_bsdf->specular(-ray.direction);
        if(specular) {
            const auto dir = specular->first;
            // avoid self-intersection by offseting origin.
            Ray new_ray(mg.pos() + epsilon * dir, dir);
            return
                specular->second.cwiseProduct(
                    trace(new_ray, sampler, depth - 1)) +
                o_bsdf->emission(-ray.direction);
        } else {
            const auto dir = sampler.uniformHemisphere(mg.normal());
            // avoid self-intersection by offseting origin.
            Ray new_ray(mg.pos() + epsilon * dir, dir);
            return
                o_bsdf->bsdf(dir, -ray.direction).cwiseProduct(
                    trace(new_ray, sampler, depth - 1)) *
                (std::abs(mg.normal().dot(dir)) * pi * pi) +
                o_bsdf->emission(-ray.direction) +
                directLight(
                    mg.pos() + epsilon * dir,
                    mg.normal(),
                    -ray.direction, *o_bsdf);
        }
    } else {
        return background_radiance;
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
