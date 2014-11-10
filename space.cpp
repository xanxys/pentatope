#include "space.h"

namespace pentatope {

physics_error::physics_error(const std::string& what) :
		std::logic_error(what) {
}

// identity.
Pose::Pose() : pose(Eigen::Transform<float, 4, Eigen::Affine>::Identity()) {
}

Eigen::Transform<float, 4, Eigen::Affine> Pose::asAffine() const {
    return pose;
}


Ray::Ray(Eigen::Vector4f origin, Eigen::Vector4f direction) :
    origin(origin), direction(direction) {
}

Eigen::Vector4f Ray::at(float t) const {
    return origin + direction * t;
}

float Ray::at(const Eigen::Vector4f& pos) const {
    return (pos - origin).dot(direction);
}

};
