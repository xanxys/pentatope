// Defines several 4-d shapes, and collection of
// useful surface properties for shading.
// Omit hyper- prefix (e.g. hyperplane, hypersphere, ...),
// since we don't care about wimpy 3-d space.
//
// Remeber, all surface is 3-d and all volume is 4-d.
#pragma once

#include <array>

#include <boost/optional.hpp>
#include <Eigen/Dense>

#include <space.h>

namespace pentatope {

// An infinitesimal part of Geometry. (i.e. a point on hypersurface)
// mainly used to represent surface near intersection points.
class MicroGeometry {
public:
    // Initialize with undef value.
    // This constructor shouldn't exist, but leave this
    // until move semantics is better-supported eveywhere.
    MicroGeometry();

    MicroGeometry(
    		const Eigen::Vector4f& pos, const Eigen::Vector4f& normal);

    // Getters.
    Eigen::Vector4f pos() const;
    Eigen::Vector4f normal() const;
private:
    // non-const to allow easy copying.
    Eigen::Vector4f _pos;
    Eigen::Vector4f _normal;
};


class AABB;


// Definition of shape in 4-d space.
class Geometry {
public:
    virtual boost::optional<MicroGeometry> intersect(const Ray& ray) const = 0;
    virtual AABB bounds() const = 0;
};


class Sphere : public Geometry {
public:
    Sphere(Eigen::Vector4f center, float radius);

    boost::optional<MicroGeometry>
        intersect(const Ray& ray) const override;

    AABB bounds() const override;
private:
    const Eigen::Vector4f center;
    const float radius;
};


// An infinite 4-d plane. Visible from both sides.
class Plane : public Geometry {
public:
    // Create a Plane {p | p.dot(normal) == d}.
    // normal must be a unit vector.
    Plane(const Eigen::Vector4f& normal, float d);

    boost::optional<MicroGeometry>
        intersect(const Ray& ray) const override;

    AABB bounds() const override;
private:
    Eigen::Vector4f normal;
    float d;
};


// An axis-aligned bounding box.
class AABB : public Geometry {
public:
    AABB(const Eigen::Vector4f& vmin, const Eigen::Vector4f& vmax);

    static AABB fromAABBs(const std::vector<AABB>& aabbs);

    // Create an AABB from vertices of a convex.
    static AABB fromConvexVertices(const std::vector<Eigen::Vector4f>& vertices);

    boost::optional<MicroGeometry>
        intersect(const Ray& ray) const override;

    AABB bounds() const override;

    // Accessors.
    Eigen::Vector4f size() const;
    Eigen::Vector4f center() const;

    Eigen::Vector4f min() const;
    Eigen::Vector4f max() const;
private:
    Eigen::Vector4f vmin;
    Eigen::Vector4f vmax;
};

// A bounded rotated cuboid.
class OBB : public Geometry {
public:
    OBB(const Pose& pose, const Eigen::Vector4f& size);

    boost::optional<MicroGeometry>
        intersect(const Ray& ray) const override;

    AABB bounds() const override;
private:
    Pose pose;
    Eigen::Vector4f half_size;
};


// Basic element of a surface.
class Tetrahedron : public Geometry {
public:
    Tetrahedron(const std::array<Eigen::Vector4f, 4>& vertices);

    boost::optional<MicroGeometry>
        intersect(const Ray& ray) const override;

    AABB bounds() const override;
private:
    std::array<Eigen::Vector4f, 4> vertices;
};

}  // namespace
