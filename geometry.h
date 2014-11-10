// Defines several 4-d shapes, and collection of
// useful surface properties for shading.
// Omit hyper- prefix (e.g. hyperplane, hypersphere, ...),
// since we don't care about wimpy 3-d space.
//
// Remeber, all surface is 3-d and all volume is 4-d.
#pragma once

#include <boost/optional.hpp>
#include <Eigen/Dense>

#include <space.h>

namespace pentatope {

// An infinitesimal part of Geometry. (i.e. a point on hypersurface)
// mainly used to represent surface near intersection points.
class MicroGeometry {
public:
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


// Definition of shape in 4-d space.
class Geometry {
public:
    virtual boost::optional<MicroGeometry> intersect(const Ray& ray) const = 0;
};


class Sphere : public Geometry {
public:
    Sphere(Eigen::Vector4f center, float radius);

    boost::optional<MicroGeometry>
        intersect(const Ray& ray) const override;
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
private:
    Eigen::Vector4f normal;
    float d;
};

};
