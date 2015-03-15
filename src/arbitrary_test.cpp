#include "arbitrary_test.h"

#include <boost/range/irange.hpp>

std::vector<pentatope::Object> arbitraryObjects(std::mt19937& rg, int n_target) {
    const int n = (n_target >= 0) ?
        n_target :
        (std::uniform_int_distribution<int>(1, 100)(rg));
    std::vector<pentatope::Object> objects;
    for(const int i : boost::irange(0, n)) {
        if(std::bernoulli_distribution(0.01)(rg)) {
            // Generate a Plane.
            Eigen::Vector4f dir(
                std::uniform_real_distribution<float>(-1, 1)(rg),
                std::uniform_real_distribution<float>(-1, 1)(rg),
                std::uniform_real_distribution<float>(-1, 1)(rg),
                std::uniform_real_distribution<float>(-1, 1)(rg));
            dir.normalize();

            objects.emplace_back(
                std::make_unique<pentatope::Plane>(
                    dir,
                    std::uniform_real_distribution<float>(-100, 100)(rg)),
                std::make_unique<pentatope::UniformLambertMaterial>(
                    pentatope::fromRgb(1, 1, 1)));
        } else {
            // Generate a Sphere.
            objects.emplace_back(
                std::make_unique<pentatope::Sphere>(
                    Eigen::Vector4f(
                        std::uniform_real_distribution<float>(-100, 100)(rg),
                        std::uniform_real_distribution<float>(-100, 100)(rg),
                        std::uniform_real_distribution<float>(-100, 100)(rg),
                        std::uniform_real_distribution<float>(-100, 100)(rg)),
                    std::uniform_real_distribution<float>(1e-3, 10)(rg)),
                std::make_unique<pentatope::UniformLambertMaterial>(
                    pentatope::fromRgb(1, 1, 1)));
        }
    }
    return objects;
}

pentatope::Ray arbitraryRay(std::mt19937& rg) {
    Eigen::Vector4f org(
        std::uniform_real_distribution<float>(-100, 100)(rg),
        std::uniform_real_distribution<float>(-100, 100)(rg),
        std::uniform_real_distribution<float>(-100, 100)(rg),
        std::uniform_real_distribution<float>(-100, 100)(rg));
    Eigen::Vector4f dir(
        std::uniform_real_distribution<float>(-1, 1)(rg),
        std::uniform_real_distribution<float>(-1, 1)(rg),
        std::uniform_real_distribution<float>(-1, 1)(rg),
        std::uniform_real_distribution<float>(-1, 1)(rg));
    dir.normalize();
    return pentatope::Ray(org, dir);
}
