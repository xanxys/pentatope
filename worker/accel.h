#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <boost/optional.hpp>
#include <Eigen/Dense>
#include <glog/logging.h>

#include <geometry.h>
#include <space.h>
#include <object.h>

namespace pentatope {

class Accel {
public:
    virtual ~Accel() {}
    // The array objects may not persist,
    // but each object must be usable during lifetime
    // of Accel. 
    virtual void build(const std::vector<Object>& objects) = 0;
    virtual std::pair<std::unique_ptr<BSDF>, MicroGeometry>
        intersect(const Ray& ray) const = 0;
};


// An "accelerator" that uses brute-force.
// Useful as the ground truth.
class BruteForceAccel : public Accel {
public:
    void build(const std::vector<Object>& objects) override;
    std::pair<std::unique_ptr<BSDF>, MicroGeometry>
        intersect(const Ray& ray) const override;
private:
    std::vector<std::reference_wrapper<const Object>> object_refs;
};


// Ray intersection accelerator using bounding volume
// hierarchy.
// See http://www.win.tue.nl/~hermanh/stack/bvh.pdf
class BVHAccel : public Accel {
public:
    void build(const std::vector<Object>& objects) override;
    std::pair<std::unique_ptr<BSDF>, MicroGeometry>
            intersect(const Ray& ray) const override;
private:
    class BVHNode {
    public:
        // Create an invalid node.
        BVHNode();

        AABB aabb;

        // Only populated when this node is a branch.
        std::unique_ptr<BVHNode> left;
        std::unique_ptr<BVHNode> right;

        // borrowed
        // empty -> this node is a branch (left && right)
        // non-empty -> this node is a leaf
        std::vector<std::reference_wrapper<const Object>> objects;
    };

    std::unique_ptr<BVHNode> buildTree(
        const std::vector<
            std::reference_wrapper<const Object>>& objects) const;

    std::pair<std::unique_ptr<BSDF>, MicroGeometry>
        intersectTree(const BVHNode& node, const Ray& ray) const;

    std::unique_ptr<BVHNode> root;
};

}  // namespace
