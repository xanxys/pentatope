#pragma once

#include <random>

#include <boost/optional.hpp>
#include <Eigen/Dense>

#include <space.h>

namespace pentatope {

class Sampler {
public:
    Sampler();

    Eigen::Vector4f uniformHemisphere(const Eigen::Vector4f& normal);
public:
    std::mt19937 gen;
};

}  // namespace
