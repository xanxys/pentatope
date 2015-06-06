#include "scene.h"

#include <algorithm>

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
    // Fallback to median splitting.
    std::vector<std::reference_wrapper<const Object>> children_sorted = objects;
    std::sort(children_sorted.begin(), children_sorted.end(),
        [&longest_axis](auto obj0, auto obj1) {
            return obj0.get().first->bounds().center()(longest_axis) <
                obj1.get().first->bounds().center()(longest_axis);
        });
    const std::size_t n0 = children_sorted.size() / 2;
    assert(n0 < children_sorted.size());
    children0.clear();
    children1.clear();
    children0.insert(children0.begin(),
            children_sorted.begin(), children_sorted.begin() + n0);
    children1.insert(children1.begin(),
            children_sorted.begin() + n0, children_sorted.end());
    assert(children0.size() + children1.size() == objects.size());
    assert(!children0.empty());
    assert(!children1.empty());
    node->left = buildTree(children0);
    node->right = buildTree(children1);
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

}  // namespace
