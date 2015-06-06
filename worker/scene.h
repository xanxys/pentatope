#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <boost/optional.hpp>
#include <Eigen/Dense>
#include <glog/logging.h>

#include <geometry.h>
#include <light.h>
#include <material.h>
#include <sampling.h>
#include <space.h>

namespace pentatope {

using Object = std::pair<
    std::unique_ptr<Geometry>,
    std::unique_ptr<Material>>;

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


// Complete collection of visually relevant things.
// Provides radiance interface (trace) externally.
class Scene {
public:
    Scene(const Spectrum& background_radiance, const boost::optional<float>& scattering_sigma);

    // Insert an Object to the Scene. It cannot be deleted once added.
    void addObject(Object object);
    // Insert an Light to the Scene.
    void addLight(std::unique_ptr<Light> light);

    // Create acceleration structure.
    // This must be called for change in objects or lights
    // to take effect. 
    void finalize();

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
private:
    std::vector<Object> objects;
    std::vector<std::unique_ptr<Light>> lights;
    Spectrum background_radiance;

    boost::optional<float> scattering_sigma;

    std::unique_ptr<Accel> accel;
};

}  // namespace
