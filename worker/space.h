// Defines mathematical constructs for 4-d space, such
// as poses and rays. Also linear algebra stuff
// useful in 4-d space.
// Don't put radiometry stuff here.
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Dense>

namespace pentatope {

const float pi = 3.14159265359;

// Use this to represent (relative) angle to avoid
// confusion over radian vs degree.
using Radianf = float;


// This error is thrown when someone tries to do physically
// impossible things.
class physics_error : public std::logic_error {
public:
    physics_error(const std::string& what);
};


// pose in 4-d space. (4 translational DoF + 6 rotational DoF)
// represented by local to parent transform.
class Pose {
public:
    // identity.
    Pose();
    // from rotation and translation. Rp + t
    Pose(const Eigen::Matrix4f& rot, const Eigen::Vector4f& trans);

    Eigen::Transform<float, 4, Eigen::Affine> asAffine() const;
    Eigen::Transform<float, 4, Eigen::Affine> asInverseAffine() const;
private:
    Eigen::Transform<float, 4, Eigen::Affine> pose;
};


// intersection range: (0, +inf)
class Ray {
public:
    Ray(Eigen::Vector4f origin, Eigen::Vector4f direction);

    // point <-> distance converions.
    Eigen::Vector4f at(float t) const;
    float at(const Eigen::Vector4f& pos) const;
public:
    const Eigen::Vector4f origin;
    const Eigen::Vector4f direction;
};


// Returns a vector that is perpendicular to
// any of given vectors.
// In other words, *(v0 ∧ v1 ∧ v2).
// n.b. results is not normalized.
Eigen::Vector4f cross(
    const Eigen::Vector4f& v0,
    const Eigen::Vector4f& v1,
    const Eigen::Vector4f& v2);

};
