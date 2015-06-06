#include "sampling.h"

#include <limits>

#include <boost/range/irange.hpp>


namespace pentatope {

Sampler::Sampler() {
}

Eigen::Vector4f Sampler::uniformHemisphere(const Eigen::Vector4f& normal) {
    std::uniform_real_distribution<float> interval(-1, 1);
    Eigen::Vector4f result;
    while(true) {
        result = Eigen::Vector4f(
            interval(gen),
            interval(gen),
            interval(gen),
            interval(gen));
        const float radius = result.norm();
        if(radius > 1 || radius == 0) {
            continue;
        }
        const float cosine = result.dot(normal);
        if(cosine >= 0) {
            result /= radius;
        } else {
            result /= -radius;
        }
        return result;
    }
}


Eigen::Vector4f Sampler::uniformSphere() {
    std::uniform_real_distribution<float> interval(-1, 1);
    Eigen::Vector4f result;
    while(true) {
        result = Eigen::Vector4f(
            interval(gen),
            interval(gen),
            interval(gen),
            interval(gen));
        const float radius = result.norm();
        if(radius > 1 || radius == 0) {
            continue;
        }
        result /= radius;
        return result;
    }
}

std::vector<Sampler> Sampler::split(int n) {
    assert(n >= 0);
    std::uniform_int_distribution<uint64_t> prob_seed(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());
    std::vector<Sampler> samplers;
    for(int i : boost::irange(0, n)) {
        Sampler s = *this;
        s.gen.seed(prob_seed(gen));
        samplers.push_back(s);
    }
    return samplers;
}

}  // namespace
