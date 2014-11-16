#include "geometry.h"

#include <cmath>

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

};
