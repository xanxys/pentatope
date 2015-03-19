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

AABB Sphere::bounds() const {
    return AABB(
        center - Eigen::Vector4f(radius, radius, radius, radius),
        center + Eigen::Vector4f(radius, radius, radius, radius));
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
    if(ray.at(t).norm() > cutoff_radius) {
        return boost::none;
    }
    // perp_dir > 0: negative side
    // perp_dir < 0: positive side
    return MicroGeometry(
        ray.at(t),
        (perp_dir > 0) ? static_cast<Eigen::Vector4f>(-normal) : normal);
}

AABB Plane::bounds() const {
    const float l = -cutoff_radius * 2;
    const float m = cutoff_radius * 2;
    return AABB(
        Eigen::Vector4f(l, l, l, l),
        Eigen::Vector4f(m, m, m, m));
}


AABB::AABB(const Eigen::Vector4f& vmin, const Eigen::Vector4f& vmax) :
        vmin(vmin), vmax(vmax) {
}

AABB AABB::fromAABBs(const std::vector<AABB>& aabbs) {
    assert(!aabbs.empty());

    const float l = std::numeric_limits<float>::lowest();
    const float m = std::numeric_limits<float>::max();
    Eigen::Vector4f vmin(m, m, m, m);
    Eigen::Vector4f vmax(l, l, l, l);

    for(const auto& aabb : aabbs) {
        vmin = vmin.cwiseMin(aabb.vmin);
        vmax = vmax.cwiseMax(aabb.vmax);
    }
    return AABB(vmin, vmax);
}

AABB AABB::fromConvexVertices(const std::vector<Eigen::Vector4f>& vertices) {
    assert(!vertices.empty());

    const float l = std::numeric_limits<float>::lowest();
    const float m = std::numeric_limits<float>::max();
    Eigen::Vector4f vmin(m, m, m, m);
    Eigen::Vector4f vmax(l, l, l, l);

    for(const auto& vertex : vertices) {
        vmin = vmin.cwiseMin(vertex);
        vmax = vmax.cwiseMax(vertex);
    }
    return AABB(vmin, vmax);
}

boost::optional<MicroGeometry>
        AABB::intersect(const Ray& ray) const {
    // AABB is an intersection of 8 half-spaces, 2 for
    // each axis.
    std::pair<float, Eigen::Vector4f> min_t_n;
    min_t_n.first = std::numeric_limits<float>::max();
    for(int i : boost::irange(0, 4)) {
        // Check two planes' t. (positive and negative)
        const float perp_dir = ray.direction(i);
        if(perp_dir == 0) {
            continue;
        }
        const float inv_perp_dir = 1.0 / perp_dir;

        //  neg     pos
        //  |        |
        //  |        |
        // vmin    vmax
        // hit point on the positive/negative plane
        // must also be within the AABB boundary.
        const float t_org = ray.origin(i);

        for(const bool is_positive : {false, true}) {
            // Check whether ray can intersect the current side of the plane.
            const float t = ((is_positive ? vmax(i) : vmin(i)) - t_org) * inv_perp_dir;
            if(t <= 0) {
                continue;
            }
            // Infinite plane intersects the ray, so proceed to boundary check.
            Eigen::Vector4f normal = Eigen::Vector4f::Zero();
            normal(i) = (is_positive ? 1 : -1);
            bool within_boundary = true;
            const Eigen::Vector4f pos_cand = ray.at(t);
            for(const int axis : boost::irange(0, 4)) {
                // Avoid instability.
                if(i == axis) {
                    continue;
                }
                within_boundary &= (vmin(axis) <= pos_cand(axis));
                within_boundary &= (pos_cand(axis) <= vmax(axis));
            }
            if(within_boundary && t < min_t_n.first) {
                min_t_n = std::make_pair(t, normal);
            }
        }
    }
    assert(min_t_n.first > 0);

    if(min_t_n.first == std::numeric_limits<float>::max()) {
        return boost::none;
    } else {
        return MicroGeometry(ray.at(min_t_n.first), min_t_n.second);
    }
}

AABB AABB::bounds() const {
    return AABB(*this);
}

bool AABB::contains(const Eigen::Vector4f& point) const {
    for(const int axis : boost::irange(0, 4)) {
        if(!(vmin(axis) <= point(axis) && point(axis) <= vmax(axis))) {
            return false;
        }
    }
    return true;
}

Eigen::Vector4f AABB::size() const {
    return vmax - vmin;
}

Eigen::Vector4f AABB::center() const {
    return (vmin + vmax) / 2;
}

Eigen::Vector4f AABB::min() const {
    return vmin;

}

Eigen::Vector4f AABB::max() const {
    return vmax;
}


OBB::OBB(const Pose& pose, const Eigen::Vector4f& size) :
        pose(pose), half_size(size / 2) {
    if(size(0) <= 0 || size(1) <= 0 || size(2) <= 0 || size(3) <= 0) {
        throw std::invalid_argument("OBB size must be positive");
    }
    world_to_local = pose.asInverseAffine();
}

boost::optional<MicroGeometry>
        OBB::intersect(const Ray& ray) const {
    const Ray ray_local(
        world_to_local * ray.origin,
        world_to_local.rotation() * ray.direction);

    // AABB is an intersection of 8 half-spaces, 2 for
    // each axis.
    std::pair<float, Eigen::Vector4f> min_t_n;
    min_t_n.first = std::numeric_limits<float>::max();
    for(int i : boost::irange(0, 4)) {
        // Check two planes' t. (positive and negative)
        const float perp_dir = ray_local.direction(i);
        if(perp_dir == 0) {
            continue;
        }
        const float inv_perp_dir = 1.0 / perp_dir;

        //  neg     pos
        //  |        |
        //  |        |
        // vmin    vmax
        // hit point on the positive/negative plane
        // must also be within the AABB boundary.
        const float t_org = ray_local.origin(i);

        for(const bool is_positive : {false, true}) {
            // Check whether ray can intersect the current side of the plane.
            const float t = ((is_positive ? half_size(i) : -half_size(i)) - t_org) * inv_perp_dir;
            if(t <= 0) {
                continue;
            }
            // Infinite plane intersects the ray, so proceed to boundary check.
            Eigen::Vector4f normal = Eigen::Vector4f::Zero();
            normal(i) = (is_positive ? 1 : -1);
            bool within_boundary = true;
            const Eigen::Vector4f pos_cand = ray_local.at(t);
            for(const int axis : boost::irange(0, 4)) {
                // Avoid instability.
                if(i == axis) {
                    continue;
                }
                within_boundary &= (std::abs(pos_cand(axis)) <= half_size(axis));
            }
            if(within_boundary && t < min_t_n.first) {
                min_t_n = std::make_pair(t, normal);
            }
        }
    }
    assert(min_t_n.first > 0);

    if(min_t_n.first == std::numeric_limits<float>::max()) {
        return boost::none;
    } else {
        return MicroGeometry(ray.at(min_t_n.first),
            pose.asAffine().rotation() * min_t_n.second);
    }
}

AABB OBB::bounds() const {
    std::vector<Eigen::Vector4f> vs;
    vs.reserve(16);
    for(const int i_vertex : boost::irange(0, 16)) {
        const Eigen::Vector4f vertex_local(
            (i_vertex & 0b0001) ? -half_size(0) : half_size(0),
            (i_vertex & 0b0010) ? -half_size(1) : half_size(1),
            (i_vertex & 0b0100) ? -half_size(2) : half_size(2),
            (i_vertex & 0b1000) ? -half_size(3) : half_size(3));
        vs.emplace_back(pose.asAffine() * vertex_local);
    }
    return AABB::fromConvexVertices(vs);
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

AABB Tetrahedron::bounds() const {
    std::vector<Eigen::Vector4f> vs(vertices.begin(), vertices.end());
    return AABB::fromConvexVertices(vs);
}

};
