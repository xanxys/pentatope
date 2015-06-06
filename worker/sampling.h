#pragma once

#include <random>

#include <boost/optional.hpp>
#include <Eigen/Dense>

#include <space.h>

namespace pentatope {

class Sampler {
public:
    Sampler();

    // Split this Sampler into independent but deterministic Samplers.
    // All children and this will be independent too.
    std::vector<Sampler> split(int n);

    Eigen::Vector4f uniformHemisphere(const Eigen::Vector4f& normal);
    Eigen::Vector4f uniformSphere();
public:
    std::mt19937 gen;
};

}  // namespace
