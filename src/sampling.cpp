#include "sampling.h"

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

}  // namespace
