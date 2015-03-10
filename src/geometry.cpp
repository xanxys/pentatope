#include "geometry.h"

#include <cmath>
#include <limits>

#include <boost/range/irange.hpp>


namespace pentatope {

MicroGeometry::MicroGeometry() {
}

MicroGeometry::MicroGeometry(const Eigen::Vector4f& pos, const Eigen::Vector4f& normal) :
        _pos(pos), _normal(normal) {
}

Eigen::Vector4f MicroGeometry::pos() const {
    return _pos;
}

Eigen::Vector4f MicroGeometry::normal() const {
    return _normal;
}


Sphere::Sphere(Eigen::Vector4f center, float radius) :
        center(center), radius(radius) {
}

boost::optional<MicroGeometry>
        Sphere::intersect(const Ray& ray) const {
    const Eigen::Vector4f delta = ray.origin - center;
    // turn into a quadratic equation at^2+bt+c=0
    const float a = std::pow(ray.direction.norm(), 2);
    const float b = 2 * delta.dot(ray.direction);
    const float c = std::pow(delta.norm(), 2) - std::pow(radius, 2);
    const float det = b * b - 4 * a * c;
    if(det < 0) {
        return boost::none;
    }
    const float t0 = (-b - std::sqrt(det)) / (2 * a);
    const float t1 = (-b + std::sqrt(det)) / (2 * a);
    float t_isect;
    if(t0 > 0) {
        t_isect = t0;
    } else if(t1 > 0) {
        t_isect = t1;
    } else {
        return boost::none;
    }
    // store intersection
    const Eigen::Vector4f p = ray.at(t_isect);
    return MicroGeometry(p, (p - center).normalized());
}


Plane::Plane(const Eigen::Vector4f& normal, float d) :
        normal(normal), d(d) {
}

boost::optional<MicroGeometry>
        Plane::intersect(const Ray& ray) const {
    const float perp_dir = normal.dot(ray.direction);
    if(perp_dir == 0) {
        return boost::none;
    }
    const float t = (d - normal.dot(ray.origin)) / perp_dir;
    if(t <= 0) {
        return boost::none;
    }
    // perp_dir > 0: negative side
    // perp_dir < 0: positive side
    return MicroGeometry(
        ray.at(t),
        (perp_dir > 0) ? static_cast<Eigen::Vector4f>(-normal) : normal);
}


AABB::AABB(const Eigen::Vector4f& vmin, const Eigen::Vector4f& vmax) :
        vmin(vmin), vmax(vmax) {
}

boost::optional<MicroGeometry>
        AABB::intersect(const Ray& ray) const {
    // OBB is an intersection of 8 half-spaces, 2 for
    // each axis.
    float min_t = std::numeric_limits<float>::max();
    Eigen::Vector4f min_normal;
    for(int i : boost::irange(0, 4)) {
        // Check two planes' t. (positive and negative)
        Eigen::Vector4f normal = Eigen::Vector4f::Zero();
        normal(i) = 1;
        const float perp_dir = normal.dot(ray.direction);
        if(perp_dir == 0) {
            continue;
        }

        //  neg     pos
        //  |        |  ----> perp_dir > 0
        //  |        |  <---- perp_dir < 0
        //
        // hit point on the positive/negative plane
        // must also be within the OBB boundary.
        // Conjecture:
        // OBB boundary test will never make a 2nd hit plane a
        // 1st hit.
        const float t_org = normal.dot(ray.origin);
        const float t_neg = (vmin(i) - t_org) / perp_dir;
        const float t_pos = (vmax(i) - t_org) / perp_dir;

        float t_cand;
        Eigen::Vector4f normal_cand;
        if(perp_dir > 0) {
            assert(t_neg < t_pos);
            if(0 >= t_pos) {
                continue;
            } else if(0 >= t_neg) {
                t_cand = t_pos;
                normal_cand = normal;
            } else {
                t_cand = t_neg;
                normal_cand = -normal;
            }
        } else {
            assert(t_neg > t_pos);
            if(0 >= t_pos) {
                continue;
            } else if(0 >= t_neg) {
                t_cand = t_neg;
                normal_cand = -normal;
            } else {
                t_cand = t_pos;
                normal_cand = -normal;
            }
        }
        const auto pos_cand = ray.at(t_cand);
        bool within_boundary = true;
        for(const int axis : boost::irange(0, 4)) {
            // Avoid instability.
            if(i == axis) {
                continue;
            }
            within_boundary &= (vmin(axis) <= pos_cand(axis));
            within_boundary &= (pos_cand(axis) <= vmax(axis));
        }
        if(within_boundary && min_t > t_cand) {
            min_t = t_cand;
            min_normal = normal_cand;
        }
    }
    assert(min_t > 0);

    if(min_t == std::numeric_limits<float>::max()) {
        return boost::none;
    } else {
        return MicroGeometry(ray.at(min_t), min_normal);
    }
}


OBB::OBB(const Pose& pose, const Eigen::Vector4f& size) :
        pose(pose), half_size(size / 2) {
    if(size(0) <= 0 || size(1) <= 0 || size(2) <= 0 || size(3) <= 0) {
        throw std::invalid_argument("OBB size must be positive");
    }
}

boost::optional<MicroGeometry>
        OBB::intersect(const Ray& ray) const {
    const Eigen::Transform<float, 4, Eigen::Affine> world_to_local =
        pose.asInverseAffine();

    const Eigen::Vector4f org_local = world_to_local * ray.origin;
    const Eigen::Vector4f dir_local = world_to_local.rotation() * ray.direction;

    // OBB is a union of 8 half-spaces, 2 for
    // each axis.
    float min_t = std::numeric_limits<float>::max();
    Eigen::Vector4f min_normal;
    for(int i : boost::irange(0, 4)) {
        // Check two planes' t. (positive and negative)
        Eigen::Vector4f normal = Eigen::Vector4f::Zero();
        normal(i) = 1;
        const float perp_dir = normal.dot(dir_local);
        if(perp_dir == 0) {
            continue;
        }

        //  neg     pos
        //  |        |  ----> perp_dir > 0
        //  |        |  <---- perp_dir < 0
        //
        // hit point on the positive/negative plane
        // must also be within the OBB boundary.
        // Conjecture:
        // OBB boundary test will never make a 2nd hit plane a
        // 1st hit.
        const float t_org = normal.dot(org_local);
        const float t_neg = (-half_size(i) - t_org) / perp_dir;
        const float t_pos = (half_size(i) - t_org) / perp_dir;

        float t_cand;
        Eigen::Vector4f normal_cand;
        if(perp_dir > 0) {
            assert(t_neg < t_pos);
            if(0 >= t_pos) {
                continue;
            } else if(0 >= t_neg) {
                t_cand = t_pos;
                normal_cand = normal;
            } else {
                t_cand = t_neg;
                normal_cand = -normal;
            }
        } else {
            assert(t_neg > t_pos);
            if(0 >= t_pos) {
                continue;
            } else if(0 >= t_neg) {
                t_cand = t_neg;
                normal_cand = -normal;
            } else {
                t_cand = t_pos;
                normal_cand = -normal;
            }
        }
        const auto pos_cand = org_local + dir_local * t_cand;
        bool within_boundary = true;
        for(const int axis : boost::irange(0, 4)) {
            // Avoid instability.
            if(i == axis) {
                continue;
            }
            within_boundary &= (
                std::abs(pos_cand(axis)) <= half_size(axis));
        }
        if(within_boundary && min_t > t_cand) {
            min_t = t_cand;
            min_normal = normal_cand;
        }
    }
    assert(min_t > 0);

    if(min_t == std::numeric_limits<float>::max()) {
        return boost::none;
    } else {
        return MicroGeometry(
            ray.at(min_t),
            pose.asAffine().rotation() * min_normal);
    }
}


Tetrahedron::Tetrahedron(
        const std::array<Eigen::Vector4f, 4>& vertices) :
        vertices(vertices) {
}

boost::optional<MicroGeometry>
        Tetrahedron::intersect(const Ray& ray) const {
    // v0 + (v1 - v0)t1 + (v2 - v0)t2 + (v3 - v0)t3 = o + dt
    // reorganaize it.
    // |v1-v0 v2-v0 v3-v0 -d| (t1 t2 t3 t)^t = o - v0
    Eigen::Matrix4f m;
    Eigen::Vector4f v;
    m.col(0) = vertices[1] - vertices[0];
    m.col(1) = vertices[2] - vertices[0];
    m.col(2) = vertices[3] - vertices[0];
    m.col(3) = -ray.direction;
    v = ray.origin - vertices[0];
    if(std::abs(m.determinant()) < 1e-6) {
        // degenerate -> ray is parallel to tetrahedron, or
        // tetrahedron itself is degenerate.
        return boost::none;
    }
    // Reject false intersections.
    const Eigen::Vector4f params = m.inverse() * v;
    for(const int i : boost::irange(0, 4)) {
        if(params(i) < 0) {
            return boost::none;
        }
    }
    if(params(0) + params(1) + params(2) > 1) {
        return boost::none;
    }
    Eigen::Vector4f n = cross(
        vertices[1] - vertices[0],
        vertices[2] - vertices[0],
        vertices[3] - vertices[0]);
    n.normalize();
    if(ray.direction.dot(n) > 0) {
        n *= -1;
    }
    return MicroGeometry(
        ray.at(params(3)),
        n);
}


};
