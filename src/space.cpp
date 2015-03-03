#include "space.h"

#include <boost/range/irange.hpp>
#include <Eigen/LU>

namespace pentatope {

physics_error::physics_error(const std::string& what) :
        std::logic_error(what) {
}

// identity.
Pose::Pose() : pose(Eigen::Transform<float, 4, Eigen::Affine>::Identity()) {
}

Pose::Pose(const Eigen::Matrix4f& rot, const Eigen::Vector4f& trans) {
    pose.linear() = rot;
    pose.translation() = trans;
}

Eigen::Transform<float, 4, Eigen::Affine> Pose::asAffine() const {
    return pose;
}

Eigen::Transform<float, 4, Eigen::Affine> Pose::asInverseAffine() const {

    Eigen::Transform<float, 4, Eigen::Affine> inv;
    inv.linear() = pose.linear().inverse().eval();
    inv.translation() = -(inv.linear() * pose.translation());
    return inv;
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


// See https://ef.gy/linear-algebra:normal-vectors-in-higher-dimensional-spaces
Eigen::Vector4f cross(
        const Eigen::Vector4f& v0,
        const Eigen::Vector4f& v1,
        const Eigen::Vector4f& v2) {
    Eigen::Vector4f result;
    for(const int i : boost::irange(0, 4)) {
        // det| v0 v1 v2 (e0 e1 e2 e3)^t| = result
        Eigen::Matrix3f m;
        int pack_index = 0;
        for(const int j : boost::irange(0, 4)) {
            if(i == j) {
                continue;
            }
            m(pack_index, 0) = v0(j);
            m(pack_index, 1) = v1(j);
            m(pack_index, 2) = v2(j);
            pack_index++;
        }
        const float sign = (i % 2 == 0) ? 1 : -1;
        result(i) = sign * m.determinant();
    }
    return result;
}

};
